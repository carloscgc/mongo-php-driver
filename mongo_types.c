//mongo_types.c
/**
 *  Copyright 2009 10gen, Inc.
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef WIN32
#include <time.h>
#else
#include <string.h>
#include <sys/time.h>
#endif

#include <php.h>

#include "mongo_types.h"
#include "mongo.h"
#include "db.h"
#include "collection.h"
#include "bson.h"

extern zend_class_entry *mongo_ce_DB,
  *spl_ce_InvalidArgumentException,
  *mongo_ce_Exception;

extern zend_object_handlers mongo_default_handlers;

zend_class_entry *mongo_ce_Date = NULL,
  *mongo_ce_BinData = NULL,
  *mongo_ce_DBRef = NULL,
  *mongo_ce_Id = NULL,
  *mongo_ce_Code = NULL,
  *mongo_ce_Regex = NULL;


void generate_id(char *data) {
  // THIS WILL ONLY WORK ON *NIX
  FILE *rand = fopen("/dev/urandom", "rb");
  char machine[4], inc[4];
  fgets(machine, 4, rand);
  fgets(inc, 4, rand);
    
  unsigned t = (unsigned) time(0);
  char *T = (char*)&t;
  data[0] = T[3];
  data[1] = T[2];
  data[2] = T[1];
  data[3] = T[0];

  memcpy(data+4, machine, 4);
  data[8] = inc[3];
  data[9] = inc[2];
  data[10] = inc[1];
  data[11] = inc[0];
  
  fclose(rand);
}

static void mongo_mongo_id_free(void *object TSRMLS_DC) {
  mongo_id *id = (mongo_id*)object;
  if (id) {
    if (id->id) {
      efree(id->id);
    }
    zend_object_std_dtor(&id->std TSRMLS_CC);
    efree(id);
  }
}

static zend_object_value mongo_mongo_id_new(zend_class_entry *class_type TSRMLS_DC) {
  zend_object_value retval;
  mongo_cursor *intern;
  zval *tmp;

  intern = emalloc(sizeof(mongo_id));
  memset(intern, 0, sizeof(mongo_id));

  zend_object_std_init(&intern->std, class_type TSRMLS_CC);
  zend_hash_copy(intern->std.properties, &class_type->default_properties, (copy_ctor_func_t) zval_add_ref, 
                 (void *) &tmp, sizeof(zval *));

  retval.handle = zend_objects_store_put(intern, (zend_objects_store_dtor_t) zend_objects_destroy_object, mongo_mongo_id_free, NULL TSRMLS_CC);
  retval.handlers = &mongo_default_handlers;

  return retval;
}

static function_entry MongoId_methods[] = {
  PHP_ME(MongoId, __construct, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MongoId, __toString, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

void mongo_init_MongoId(TSRMLS_D) {
  zend_class_entry id; 
  INIT_CLASS_ENTRY(id, "MongoId", MongoId_methods); 
  id.create_object = mongo_mongo_id_new;
  mongo_ce_Id = zend_register_internal_class(&id TSRMLS_CC); 
}

/* {{{ MongoId::__construct()
 */
PHP_METHOD(MongoId, __construct) {
  zval *id = 0;
  mongo_id *this_id = (mongo_id*)zend_object_store_get_object(getThis() TSRMLS_CC);
  this_id->id = (char*)emalloc(OID_SIZE);

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &id) == FAILURE) {
    return;
  }
  
  if (id && 
      Z_TYPE_P(id) == IS_STRING && 
      Z_STRLEN_P(id) == 24) {
    int i;
    for(i=0;i<12;i++) {
      char digit1 = Z_STRVAL_P(id)[i*2], digit2 = Z_STRVAL_P(id)[i*2+1];
      digit1 = digit1 >= 'a' && digit1 <= 'f' ? digit1 -= 87 : digit1;
      digit1 = digit1 >= 'A' && digit1 <= 'F' ? digit1 -= 55 : digit1;
      digit1 = digit1 >= '0' && digit1 <= '9' ? digit1 -= 48 : digit1;
      
      digit2 = digit2 >= 'a' && digit2 <= 'f' ? digit2 -= 87 : digit2;
      digit2 = digit2 >= 'A' && digit2 <= 'F' ? digit2 -= 55 : digit2;
      digit2 = digit2 >= '0' && digit2 <= '9' ? digit2 -= 48 : digit2;
      
      this_id->id[i] = digit1*16+digit2;
    }
  }
  else if (id && 
           Z_TYPE_P(id) == IS_OBJECT &&
           Z_OBJCE_P(id) == mongo_ce_Id) {
    mongo_id *that_id = (mongo_id*)zend_object_store_get_object(id TSRMLS_CC);
    memcpy(this_id->id, that_id->id, OID_SIZE);
  }
  else {
    generate_id(this_id->id);
  }
}
/* }}} */


/* {{{ MongoId::__toString() 
 */
PHP_METHOD(MongoId, __toString) {
  int i;
  mongo_id *this_id = (mongo_id*)zend_object_store_get_object(getThis() TSRMLS_CC);
  //  zval *zid = zend_read_property(mongo_ce_Id, getThis(), "id", 2, NOISY TSRMLS_CC);
  char *foo = this_id->id; //zid->value.str.val;

  char id[24];
  char *n = id;
  for(i=0; i<12; i++) {
    int x = *foo;
    if (*foo < 0) {
      x = 256 + *foo;
    }
    sprintf(n, "%02x", x);
    n += 2;
    foo++;
  }

  RETURN_STRING(id, DUP);
}
/* }}} */


/* {{{ MongoDate::__construct
 */
PHP_METHOD(MongoDate, __construct) {
  struct timeval time;
  long sec, usec;

  int argc = ZEND_NUM_ARGS();
  switch(argc) {
  case 0: {
#ifdef WIN32
    SYSTEMTIME systime;
	GetSystemTime(&systime);
	time.tv_sec = systime.wSecond;
	time.tv_usec = systime.wMilliseconds * 1000;
#else
    gettimeofday(&time, NULL);
#endif
    add_property_long( getThis(), "sec", time.tv_sec );
    add_property_long( getThis(), "usec", time.tv_usec );
    break;
  }
  case 1: {
    if (zend_parse_parameters(argc TSRMLS_CC, "l", &sec) == SUCCESS) {
      long long temp = sec;
      long long t_sec = temp/1000;
      long long t_usec = temp*1000 % 1000000;

      // back to longs
      sec = (long)t_sec;
      usec = (long)t_usec;

      add_property_long( getThis(), "sec", sec);
      add_property_long( getThis(), "usec", usec);
    }
    break;
  }
  case 2: {
    if (zend_parse_parameters(argc TSRMLS_CC, "ll", &sec, &usec) == SUCCESS) {
      add_property_long(getThis(), "sec", sec);
      add_property_long(getThis(), "usec", usec);
    }
    break;
  }
  }
}
/* }}} */


/* {{{ MongoDate::__toString() 
 */
PHP_METHOD(MongoDate, __toString) {
  zval *zsec = zend_read_property( mongo_ce_Date, getThis(), "sec", 3, 0 TSRMLS_CC );
  long sec = Z_LVAL_P( zsec );
  zval *zusec = zend_read_property( mongo_ce_Date, getThis(), "usec", 4, 0 TSRMLS_CC );
  long usec = Z_LVAL_P( zusec );
  double dusec = (double)usec/1000000;

  return_value->type = IS_STRING;
  return_value->value.str.len = spprintf(&(return_value->value.str.val), 0, "%.8f %ld", dusec, sec);
}
/* }}} */


static function_entry MongoDate_methods[] = {
  PHP_ME(MongoDate, __construct, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MongoDate, __toString, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

void mongo_init_MongoDate(TSRMLS_D) {
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "MongoDate", MongoDate_methods);
  mongo_ce_Date = zend_register_internal_class(&ce TSRMLS_CC);

  zend_declare_property_long(mongo_ce_Date, "sec", strlen("sec"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_long(mongo_ce_Date, "usec", strlen("usec"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
}

/* {{{ MongoBinData::__construct() 
 */
PHP_METHOD(MongoBinData, __construct) {
  char *bin;
  int bin_len, type;

  int argc = ZEND_NUM_ARGS();
  if (argc == 1 &&
      zend_parse_parameters(argc TSRMLS_CC, "s", &bin, &bin_len) != FAILURE) {
    type = BIN_BYTE_ARRAY;
  }
  else if (argc != 2 ||
           zend_parse_parameters(argc TSRMLS_CC, "sl", &bin, &bin_len, &type) == FAILURE) {
    zend_error( E_ERROR, "incorrect parameter types, expected __construct(string, long)" );
  }
  
  add_property_long( getThis(), "length", bin_len);
  add_property_stringl( getThis(), "bin", bin, bin_len, 1 );
  add_property_long( getThis(), "type", type);
}
/* }}} */


/* {{{ MongoBinData::__toString() 
 */
PHP_METHOD(MongoBinData, __toString) {
  RETURN_STRING( "<Mongo Binary Data>", 1 );
}
/* }}} */


static function_entry MongoBinData_methods[] = {
  PHP_ME(MongoBinData, __construct, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MongoBinData, __toString, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

void mongo_init_MongoBinData(TSRMLS_D) {
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "MongoBinData", MongoBinData_methods);
  mongo_ce_BinData = zend_register_internal_class(&ce TSRMLS_CC);

  zend_declare_property_string(mongo_ce_BinData, "bin", strlen("bin"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_long(mongo_ce_BinData, "length", strlen("length"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_long(mongo_ce_BinData, "type", strlen("type"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
}

/* {{{ MongoRegex::__construct() 
 */
PHP_METHOD(MongoRegex, __construct) {
  zval *regex;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &regex) == FAILURE) {
    return;
  }

  if (Z_TYPE_P(regex) == IS_OBJECT &&
      Z_OBJCE_P(regex) == mongo_ce_Regex) {
    zval *oregex = zend_read_property(mongo_ce_Regex, regex, "regex", strlen("regex"), NOISY TSRMLS_CC);
    zval_add_ref(&oregex);
    zend_update_property(mongo_ce_Regex, getThis(), "regex", strlen("regex"), oregex TSRMLS_CC);
    zval *oflags = zend_read_property(mongo_ce_Regex, regex, "flags", strlen("flags"), NOISY TSRMLS_CC);
    zval_add_ref(&oflags);
    zend_update_property(mongo_ce_Regex, getThis(), "flags", strlen("flags"), oflags TSRMLS_CC);
  }
  else if (Z_TYPE_P(regex) == IS_STRING) {
    char *re = Z_STRVAL_P(regex);
    char *eopattern = strrchr(re, '/');

    if (!eopattern) {
      zend_throw_exception(mongo_ce_Exception, "invalid regex", 0 TSRMLS_CC);
      return;
    }

    int pattern_len = eopattern - re - 1;

    if (pattern_len < 0) {
      zend_throw_exception(mongo_ce_Exception, "invalid regex", 0 TSRMLS_CC);
      return;
    }

    // move beyond the second '/' in /foo/bar 
    eopattern++;
    int flags_len = Z_STRLEN_P(regex) - (eopattern-re);

    add_property_stringl( getThis(), "regex", re+1, pattern_len, 1);
    add_property_stringl( getThis(), "flags", eopattern, flags_len, 1);
  }
  else {
    zend_throw_exception(spl_ce_InvalidArgumentException, "MongoRegex::__construct(): argument must be a string or MongoRegex", 0 TSRMLS_CC);
    return;
  }
}
/* }}} */


/* {{{ MongoRegex::__toString() 
 */
PHP_METHOD(MongoRegex, __toString) {
  zval *zre = zend_read_property( mongo_ce_Regex, getThis(), "regex", 5, 0 TSRMLS_CC );
  char *re = Z_STRVAL_P( zre );
  zval *zopts = zend_read_property( mongo_ce_Regex, getThis(), "flags", 5, 0 TSRMLS_CC );
  char *opts = Z_STRVAL_P( zopts );

  char *field_name;

  spprintf(&field_name, 0, "/%s/%s", re, opts );
  RETVAL_STRING(field_name, 1);
  efree(field_name);
}
/* }}} */


static function_entry MongoRegex_methods[] = {
  PHP_ME(MongoRegex, __construct, NULL, ZEND_ACC_PUBLIC)
  PHP_ME(MongoRegex, __toString, NULL, ZEND_ACC_PUBLIC)
  { NULL, NULL, NULL }
};

void mongo_init_MongoRegex(TSRMLS_D) {
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "MongoRegex", MongoRegex_methods);
  mongo_ce_Regex = zend_register_internal_class(&ce TSRMLS_CC);

  zend_declare_property_string(mongo_ce_Regex, "regex", strlen("regex"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_string(mongo_ce_Regex, "flags", strlen("flags"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
}



/* {{{ MongoCode::__construct(string) 
 */
PHP_METHOD(MongoCode, __construct) {
  zval *code = 0, *zcope = 0;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|a", &code, &zcope) == FAILURE) {
    return;
  }
  convert_to_string(code);

  zend_update_property(mongo_ce_Code, getThis(), "code", strlen("code"), code TSRMLS_CC);

  if (!zcope) {
    ALLOC_INIT_ZVAL(zcope);
    array_init(zcope);
  }
  else {
    zval_add_ref(&zcope);
  }

  zend_update_property(mongo_ce_Code, getThis(), "scope", strlen("scope"), zcope TSRMLS_CC);

  // get rid of extra ref
  zval_ptr_dtor(&zcope);
}
/* }}} */


/* {{{ MongoCode::__toString() 
 */
PHP_METHOD(MongoCode, __toString ) {
  zval *zode = zend_read_property(mongo_ce_Code, getThis(), "code", 4, 0 TSRMLS_CC);
  RETURN_STRING(Z_STRVAL_P(zode), 1 );
}
/* }}} */


static function_entry MongoCode_methods[] = {
  PHP_ME(MongoCode, __construct, NULL, ZEND_ACC_PUBLIC )
  PHP_ME(MongoCode, __toString, NULL, ZEND_ACC_PUBLIC )
  { NULL, NULL, NULL }
};

void mongo_init_MongoCode(TSRMLS_D) {
  zend_class_entry ce; 
  INIT_CLASS_ENTRY(ce, "MongoCode", MongoCode_methods); 
  mongo_ce_Code = zend_register_internal_class(&ce TSRMLS_CC); 

  zend_declare_property_string(mongo_ce_Code, "code", strlen("code"), "", ZEND_ACC_PUBLIC TSRMLS_CC);
  zend_declare_property_null(mongo_ce_Code, "scope", strlen("scope"), ZEND_ACC_PUBLIC TSRMLS_CC);
}

/* {{{ MongoCode::create()
 */
PHP_METHOD(MongoDBRef, create) {
  zval *zns, *zid;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &zns, &zid) == FAILURE) {
    return;
  }
  convert_to_string(zns);

  array_init(return_value);
  add_assoc_zval(return_value, "$ref", zns); 
  add_assoc_zval(return_value, "$id", zid); 
}
/* }}} */

/* {{{ MongoCode::isRef()
 */
PHP_METHOD(MongoDBRef, isRef) {
  zval *ref;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &ref) == FAILURE) {
    return;
  }

  if (zend_hash_find(Z_ARRVAL_P(ref), "$ref", 5, NULL) == SUCCESS &&
      zend_hash_find(Z_ARRVAL_P(ref), "$id", 4, NULL) == SUCCESS) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}
/* }}} */

/* {{{ MongoCode::get()
 */
PHP_METHOD(MongoDBRef, get) {
  zval *db, *ref;
  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Oa", &db, mongo_ce_DB, &ref) == FAILURE) {
    return;
  }

  zval **ns, **id;
  if (zend_hash_find(Z_ARRVAL_P(ref), "$ref", 5, (void**)&ns) == FAILURE ||
      zend_hash_find(Z_ARRVAL_P(ref), "$id", 4, (void**)&id) == FAILURE) {
    RETURN_NULL();
  }

  zval *collection;
  MAKE_STD_ZVAL(collection);

  PUSH_PARAM(*ns); PUSH_PARAM((void*)1);
  PUSH_EO_PARAM();
  zim_MongoDB_selectCollection(1, collection, &collection, db, return_value_used TSRMLS_CC);
  POP_EO_PARAM();
  POP_PARAM(); POP_PARAM();

  zval *query;
  MAKE_STD_ZVAL(query);
  array_init(query);
  add_assoc_zval(query, "_id", *id);

  PUSH_PARAM(query); PUSH_PARAM((void*)1);
  PUSH_EO_PARAM();
  zim_MongoCollection_findOne(1, return_value, return_value_ptr, collection, return_value_used TSRMLS_CC);
  POP_EO_PARAM();
  POP_PARAM(); POP_PARAM();

  zval_ptr_dtor(&collection);
  zval_ptr_dtor(&query);
}
/* }}} */

static function_entry MongoDBRef_methods[] = {
  PHP_ME(MongoDBRef, create, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
  PHP_ME(MongoDBRef, isRef, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
  PHP_ME(MongoDBRef, get, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
};


void mongo_init_MongoDBRef(TSRMLS_D) {
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "MongoDBRef", MongoDBRef_methods);
  mongo_ce_DBRef = zend_register_internal_class(&ce TSRMLS_CC);

  zend_declare_property_string(mongo_ce_DBRef, "refKey", strlen("refKey"), "$ref", ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
  zend_declare_property_string(mongo_ce_DBRef, "idKey", strlen("idKey"), "$id", ZEND_ACC_PROTECTED|ZEND_ACC_STATIC TSRMLS_CC);
}
