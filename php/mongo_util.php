<?php
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
 *
 * PHP version 5 
 *
 * @category Database
 * @package  Mongo
 * @author   Kristina Chodorow <kristina@10gen.com>
 * @license  http://www.apache.org/licenses/LICENSE-2.0  Apache License 2
 * @version  CVS: 000000
 * @link     http://www.mongodb.org
 */

/**
 * Handy methods for programming with this driver.
 * 
 * @category Database
 * @package  Mongo
 * @author   Kristina Chodorow <kristina@10gen.com>
 * @license  http://www.apache.org/licenses/LICENSE-2.0  Apache License 2
 * @link     http://www.mongodb.org
 */
class MongoUtil
{
  
    /**
     * Turns something into an array that can be saved to the db.
     * Returns the empty array if passed null.
     *
     * @param any $obj object to convert
     *
     * @return array the array
     */
    public static function objToArray($obj) 
    {
        if (is_null($obj)) {
            return array();
        }

        $arr = array();
        foreach ($obj as $key => $value) {
            if (is_object($value) || is_array($value)) {
                $arr[ $key ] = MongoUtil::objToArray($value);
            } else {
                $arr[ $key ] = $value;
            }
        }
        return $arr;
    }

    /**
     * Converts a field or array of fields into an underscore-separated string.
     *
     * @param string|array $keys field(s) to convert
     *
     * @return string the index name
     */
    public static function toIndexString($keys) 
    {
        if (is_string($keys)) {
            $name = str_replace(".", "_", $keys) + "_1";
        } else if (is_array($keys) || is_object($keys)) {
            $key_list = array();
            foreach ($keys as $k=>$v) {
                array_push($key_list, str_replace(".", "_", $k) . "_1");
            }
            $name = implode("_", $key_list);
        }
        return $name;
    }

    /** Execute a db command.
     *
     * @param conn   $conn database connection
     * @param array  $data the query to send
     * @param string $db   the database name
     *
     * @return array database response
     */
    public static function dbCommand($conn, $data, $db) 
    {
        $cmd_collection = $db . MongoUtil::$_CMD;
        $obj            = mongo_find_one($conn, $cmd_collection, $data);

        if ($obj) {
            return $obj;
        } else {
            trigger_error("no db response", E_USER_WARNING);
            return false;
        }
    }

    /**
     * Parse boolean configuration settings from php.ini.
     *
     * @param string $str the setting name
     *
     * @return bool the value of the setting
     */
    public static function getConfig($str) 
    {
        $setting = get_cfg_var($str);
        if (!$setting || strcasecmp($setting, "off") == 0) {
            return false;
        }
        return true;
    }

    /* Command collection */
    private static $_CMD = ".\$cmd";

    /* Admin database */
    public static $ADMIN = "admin";

    /* Commands */
    public static $AUTHENTICATE      = "authenticate";
    public static $CREATE_COLLECTION = "create";
    public static $DELETE_INDICES    = "deleteIndexes";
    public static $DROP              = "drop";
    public static $DROP_DATABASE     = "dropDatabase";
    public static $FORCE_ERROR       = "forceerror";
    public static $INDEX_INFO        = "cursorInfo";
    public static $LAST_ERROR        = "getlasterror";
    public static $LIST_DATABASES    = "listDatabases";
    public static $LOGGING           = "opLogging";
    public static $LOGOUT            = "logout";
    public static $NONCE             = "getnonce";
    public static $PREV_ERROR        = "getpreverror";
    public static $PROFILE           = "profile";
    public static $QUERY_TRACING     = "queryTraceLevel";
    public static $REPAIR_DATABASE   = "repairDatabase";
    public static $RESET_ERROR       = "reseterror";
    public static $SHUTDOWN          = "shutdown";
    public static $TRACING           = "traceAll";
    public static $VALIDATE          = "validate";

}



/**
 * This class can be used to create lightweight links between objects
 * in different collections.
 *
 * @category Database
 * @package  Mongo
 * @author   Kristina Chodorow <kristina@10gen.com>
 * @license  http://www.apache.org/licenses/LICENSE-2.0  Apache License 2
 * @link     http://www.mongodb.org
 */
class MongoDBRef
{
    private static $_refKey = '$ref';
    private static $_idKey  = '$id';
     
    /**
     * Creates a new db reference.
     *
     * @param string $ns the name of the collection
     * @param mixed  $id the _id of the object
     *
     * @return array a new db ref
     */ 
    public static function create($ns, $id) 
    {
        return array(MongoDBRef::$_refKey => "$ns",
                     MongoDBRef::$_idKey => $id);
    }

    /**
     * Checks if an object is a db ref
     *
     * @param array $obj object to check
     * 
     * @return bool if the object is a db ref
     */
    public static function isRef($obj) 
    {
        if (is_array($obj) && 
            array_key_exists(MongoDBRef::$_refKey, $obj) &&
            array_key_exists(MongoDBRef::$_idKey, $obj)) {
            return true;
        }
        return false;
    }

    /**
     * Gets the value a db ref points to.
     *
     * @param MongoDB $db  database to use
     * @param array   $ref database reference
     *
     * @return array the object the db ref points to or null
     */
    public static function get(MongoDB $db, $ref) 
    {
        return $db->selectCollection($ref[MongoDBRef::$_refKey])->
          findOne(array("_id" => $ref[MongoDBRef::$_idKey]));
    }
}

/**
 * Less than.
 */
define("MONGO_LT", '$lt');

/**
 * Less than or equal to.
 */
define("MONGO_LTE", '$lte');

/**
 * Greater than.
 */
define("MONGO_GT", '$gt');

/**
 * Greater than or equal to.
 */
define("MONGO_GTE", '$gte');

/**
 * Checks for a field in an object.
 */
define("MONGO_IN", '$in');

/**
 * Not equal.
 */
define("MONGO_NE", '$ne');


/**
 * Sort ascending.
 */
define("MONGO_ASC", 1);

/**
 * Sort descending.
 */
define("MONGO_DESC", -1);


/**
 * Function as binary data.
 */
define("MONGO_BIN_FUNCTION", 1);

/**
 * Default binary type: an arrray of binary data.
 */
define("MONGO_BIN_ARRAY", 2);

/**
 * Universal unique id.
 */
define("MONGO_BIN_UUID", 3);

/**
 * Binary MD5.
 */
define("MONGO_BIN_MD5", 5);

/**
 * User-defined binary type.
 */
define("MONGO_BIN_CUSTOM", 128);

?>