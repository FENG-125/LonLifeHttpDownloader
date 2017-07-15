#include "light_json.h"



int light_parse(light_value *v, const char* json)
{
	light_context c;
	int ret;
    assert(v != NULL);

	c.stack = NULL;
    c.size = 0;
    c.top = 0;
    
    c.json = json;
    v->type = LIGHT_NULL;
    light_parse_whitespace(&c);
    if ((ret = light_parse_value(&c, v)) == LIGHT_PARSE_OK) 
    {
        light_parse_whitespace(&c);
        if (*c.json != '\0')
            ret = LIGHT_PARSE_ROOT_NOT_SINGULAR;
    }

    assert(c.top == 0);
    free(c.stack);
    
    return ret;
}

static void light_parse_whitespace(light_context* c) 
{
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int light_parse_literal(light_context* c, light_value* v,
								const char* literal, light_type type)
{
    EXPECT(c, literal[0]);
    size_t i;
    for(i = 0; literal[i+1]; ++i)
    {
	    if(c->json[i] != literal[i+1])
	    return LIGHT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = type;
    return LIGHT_PARSE_OK;
}

static int light_parse_number(light_context *c, light_value *v)
{//?
	char* end;
	
  	v->munion.number = strtod(c->json, &end);
  	if (errno == ERANGE && (v->munion.number == HUGE_VAL || v->munion.number == -HUGE_VAL))
        return LIGHT_PARSE_NUMBER_TOO_BIG;
    if (c->json == end)
        return LIGHT_PARSE_INVALID_VALUE;
    
    c->json = end;
    v->type = LIGHT_NUMBER;
    return LIGHT_PARSE_OK;	
}

static int light_parse_string(light_context *c, light_value *v)
{
	size_t head = c->top; 
	size_t len;
    const char* p;
    unsigned u ,u2;
	EXPECT(c,'\"');

	p = c->json;
	while(1)
	{
		char ch = *p++;
		switch(ch)
		{
			case ' \" ':
				len = c->top - head;
				light_set_string(v, (const char*)light_context_pop(c, len), len);
				
                c->json = p;
                return LIGHT_PARSE_OK;
            case '\0':
            	c->top = head;
            	return LIGHT_PARSE_MISS_QUOTATION_MARK;
            case '\\':
            	switch(*p++)
            	{
	            	case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                    	if (!(p = light_parse_hex4(p, &u)))
						    STRING_ERROR(LIGHT_PARSE_INVALID_UNICODE_HEX);
						if (u >= 0xD800 && u <= 0xDBFF) 
						{ //�ߴ�����
						    if (*p++ != '\\')//
						        STRING_ERROR(LIGHT_PARSE_INVALID_UNICODE_SURROGATE);
						    if (*p++ != 'u')
						        STRING_ERROR(LIGHT_PARSE_INVALID_UNICODE_SURROGATE);
						    if (!(p = light_parse_hex4(p, &u2)))
						        STRING_ERROR(LIGHT_PARSE_INVALID_UNICODE_HEX);
						    if (u2 < 0xDC00 || u2 > 0xDFFF)
						        STRING_ERROR(LIGHT_PARSE_INVALID_UNICODE_SURROGATE);
						    u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
						}//x*0x400
    					light_encode_utf8(c, u);break;
                    default:
                        c->top = head;
                        return LIGHT_PARSE_INVALID_STRING_ESCAPE;
            	}break;
            default :
            	if ((unsigned char)ch < 0x20) { //�������Ϸ��ַ�
                    c->top = head;
                    return LIGHT_PARSE_INVALID_STRING_CHAR;
                }
            	PUTC(c,ch);
		}
	}
}

static int light_parse_value(light_context* c, light_value* v) 
{
    switch (*c->json) {
	    case 't':  return light_parse_literal(c,v,"true",LIGHT_TRUE);
	    case 'f':  return light_parse_literal(c,v,"false",LIGHT_FALSE);
        case 'n':  return light_parse_literal(c, v,"null",LIGHT_NULL);
        case '\0': return LIGHT_PARSE_EXPECT_VALUE;
        case '\"': return light_parse_string(c,v);
        case '[':  return light_parse_array(c,v);
        case '{':  return light_parse_object(c,v);
        default:   return light_parse_number(c, v);
    }
}

void light_free(light_value *v)
{
	size_t i;
	assert(v != NULL);
	switch(v->type)
	{
		case LIGHT_STRING:free(v->munion.str.str);break;
		case LIGHT_ARRAY:
			for(i = 0; i<v->munion.arr.size; ++i)
			{
				light_free(&v->munion.arr.arr[i]);
			}
			free(v->munion.arr.arr);
			break;
		case LIGHT_OBJECT:
            clear_map(v->munion.object.object->pmap);
            break;
		default: break;
	}
	
	v->type = LIGHT_NULL;
}

const char* light_get_string(const light_value *v)
{
	assert(v != NULL && v->type == LIGHT_STRING);
	return v->munion.str.str;
}
size_t light_get_string_length(const light_value *v)
{
	assert(v != NULL && v->type == LIGHT_STRING);
	return v->munion.str.len;
}
void light_set_string(light_value *v, const char *s, size_t len)
{
	assert(v != NULL &&(s != NULL || len != 0));
	light_free(v);
	v->munion.str.str = realloc(v->munion.str.str,len+1);
	memcpy(v->munion.str.str,s,len);
	v->munion.str.str[len] = '\0';
	v->munion.str.len = len;
	v->type = LIGHT_STRING;
}


light_type light_get_type(const light_value* v)
{
	assert(v != NULL);
	return v->type;
}

double light_get_number(const light_value* v)
{
	assert(v != NULL && v->type == LIGHT_NUMBER);
	return v->munion.number; 
}

void light_set_number(light_value *v, double n)
{
	light_free(v);
	v->munion.number = n;
	v->type = LIGHT_NUMBER;
}
int light_get_boolean(const light_value *v)
{
	assert(v != NULL && (v->type == LIGHT_TRUE|| v->type == LIGHT_FALSE));
	return v->type == LIGHT_TRUE;
}
void light_set_boolean(light_value *v, int b)
{
	light_free(v);
	v->type = b ? LIGHT_TRUE:LIGHT_FALSE;
}


static void* light_context_push(light_context *c, size_t size)
{
	void *ret;
	assert(c != NULL && size > 0);

	if(c->top + size >= c->size)
	{
		if(c->size == 0)
		{
			c->size = LIGHT_CONTEXT_STACK_INIT_SIZE;
		}
		while (c->top + size >= c->size) 
		{
			c->size += c->size>>1; 	
		}

		c->stack = (char*)realloc(c->stack, c->size);
	}
	ret = c->stack + c->top;
	c->top += size; 
	return ret;
}
static void* light_context_pop(light_context *c, size_t size)
{
	assert(c != NULL && c->top > size );
	return c->stack+(c->top -= size);
}

//���ַ����н���4λ��16������   
static const char* light_parse_hex4(const char* p, unsigned* u) 
{
    int i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

static void light_encode_utf8(light_context* c, unsigned u) 
{
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) 
    {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));//0x00 111111 ȡ��6λ
    }
    else if (u <= 0xFFFF) 
    {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else 
    {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

size_t light_get_array_size(const light_value *v)
{
	assert(v != NULL && v->type == LIGHT_ARRAY);
	return v->munion.arr.size;
}
light_value* light_get_array(const light_value *v, size_t index)
{
	assert(v != NULL && v->type == LIGHT_ARRAY);
	assert(index < v->munion.arr.size);
	return &v->munion.arr.arr[index];
}


static int light_parse_array(light_context* c, light_value* v)
{
	size_t size = 0;
	size_t i;
    int ret;
    EXPECT(c, '[');
    light_parse_whitespace(c);           //'[' ֮��
    if (*(c->json) == ']') {
        c->json++;
        v->type = LIGHT_ARRAY;
        v->munion.arr.size = 0;
        v->munion.arr.arr = NULL;
        return LIGHT_PARSE_OK;
    }
    for (;;) 
    {
        light_value e;
        light_init(&e);
        if ((ret = light_parse_value(c, &e)) != LIGHT_PARSE_OK)break;
   
        memcpy(light_context_push(c, sizeof(light_value)), &e, sizeof(light_value));//ѹ��stack
        size++;
        light_parse_whitespace(c);       //','֮ǰ
        if (*c->json == ',')
        {
	        c->json++;
	        light_parse_whitespace(c);  //','֮��
        }
            
        else if (*c->json == ']') 
        {
            c->json++;
            v->type = LIGHT_ARRAY;
            v->munion.arr.size = size;
            size *= sizeof(light_value);
            memcpy(v->munion.arr.arr = (light_value*)malloc(size), 
                                        light_context_pop(c, size), size);
            return LIGHT_PARSE_OK;
        }
        else
            ret =  LIGHT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
    }
	for (i = 0; i < size; i++)
    	light_free((light_value*)light_context_pop(c, sizeof(light_value)));
    return ret;
}


static int light_parse_object(light_context* c, light_value* v) 
{
    size_t size;
    member m;
    char *mstr = NULL;
    
    int ret;
    EXPECT(c, '{');
    light_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = LIGHT_OBJECT;
        v->munion.object.object->pmap = NULL;
        v->munion.object.size = 0;
        return LIGHT_PARSE_OK;
    }
  
    size = 0;
    for (;;) 
    {
	    light_value mkey;//
	    light_value mvalue;//  
	    size_t mlen;
	    
        light_init(&mkey);
        light_init(&mvalue);
        
        // parse key 
        if (*c->json != '"') //?
        {
            ret = LIGHT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = light_parse_string(c, &mkey)) != LIGHT_PARSE_OK)
            break;
        mlen = mkey.munion.str.len;
        mstr = mkey.munion.str.str;

        // parse ws colon ws 
   		light_parse_whitespace(c);
        if (*c->json != ':') 
        {
            ret = LIGHT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        light_parse_whitespace(c);
        
        // parse value
        if ((ret = light_parse_value(c, &mvalue)) != LIGHT_PARSE_OK)break;
        add_item(v->munion.object.object->pmap,New_Item(mstr,&mvalue));//
		mstr = NULL;
       	size++;
        
        // parse ws [',' | '}'] ws 
        light_parse_whitespace(c);
        if (*c->json == ',') 
        {
            c->json++;
            light_parse_whitespace(c);
        }
        else if (*c->json == '}') 
        {
            c->json++;
            v->type = LIGHT_OBJECT;
            v->munion.object.size = size;
            return LIGHT_PARSE_OK;
        }
        else 
        {
            ret = LIGHT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    // free  
    free(mstr);
    light_free(v);//free����
    return ret;
}




/***********************************������generate********************************************/

//�����ַ���
static void light_generate_string(light_context* c, const char* s, size_t len) 
{
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = light_context_push(c, size = len * 6 + 2);
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) 
        {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) // 
                {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}


static void light_generate_value(light_context* c,const light_value *v)
{
	size_t i;
	switch(v->type)
	{
		case LIGHT_ARRAY:
			PUTC(c,'[');	
			for(i = 0; i<v->munion.arr.size; ++i)
			{
				if(i > 0)PUTC(c,',');
				light_generate_value(c, &v->munion.arr.arr[i]);
			}
			PUTC(c,']');break;
		case LIGHT_OBJECT:
			PUTC(c,'{');
	}
}




/***********************************map*******************************************************/

Item *New_Item(char *key, void *value) 
{
	Item *pres = (Item *)calloc(sizeof(Item), 1);
	pres->key = key;
	pres->value = (char *)calloc(sizeof(char), sizeof(light_value));
	memcpy(pres->value, value,sizeof(light_value));
	return pres;
}
void show_node(void *data)
{
	Item *p = (Item *)data;
	printf("%s : %s\n", p->key, p->value);
}
void map_show(Map *pmap) 
{
	show(pmap->tree, show_node);
}
void clear_node(void *p) 
{
	light_value * pv= (light_value *)p;
	light_free(pv);
}
void clear_map(Map *pmap) {
	clear(pmap->tree, clear_node);
}