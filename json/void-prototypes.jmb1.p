diff -urw json-c-0.7/debug.h json-c-0.7-jmb/debug.h
--- json-c-0.7/debug.h	2007-03-13 08:25:39.000000000 +0000
+++ json-c-0.7-jmb/debug.h	2007-06-22 23:52:37.000000000 +0100
@@ -13,7 +13,7 @@
 #define _DEBUG_H_
 
 extern void mc_set_debug(int debug);
-extern int mc_get_debug();
+extern int mc_get_debug(void);
 
 extern void mc_set_syslog(int syslog);
 extern void mc_abort(const char *msg, ...);
diff -urw json-c-0.7/json_object.h json-c-0.7-jmb/json_object.h
--- json-c-0.7/json_object.h	2007-03-13 08:25:39.000000000 +0000
+++ json-c-0.7-jmb/json_object.h	2007-06-22 23:53:10.000000000 +0100
@@ -98,7 +98,7 @@
 /** Create a new empty object
  * @returns a json_object of type json_type_object
  */
-extern struct json_object* json_object_new_object();
+extern struct json_object* json_object_new_object(void);
 
 /** Get the hashtable of a json_object of type json_type_object
  * @param obj the json_object instance
@@ -167,7 +167,7 @@
 /** Create a new empty json_object of type json_type_array
  * @returns a json_object of type json_type_array
  */
-extern struct json_object* json_object_new_array();
+extern struct json_object* json_object_new_array(void);
 
 /** Get the arraylist of a json_object of type json_type_array
  * @param obj the json_object instance
diff -urw json-c-0.7/json_tokener.h json-c-0.7-jmb/json_tokener.h
--- json-c-0.7/json_tokener.h	2007-03-13 08:25:39.000000000 +0000
+++ json-c-0.7-jmb/json_tokener.h	2007-06-22 23:53:26.000000000 +0100
@@ -79,7 +79,7 @@
 
 extern const char* json_tokener_errors[];
 
-extern struct json_tokener* json_tokener_new();
+extern struct json_tokener* json_tokener_new(void);
 extern void json_tokener_free(struct json_tokener *tok);
 extern void json_tokener_reset(struct json_tokener *tok);
 extern struct json_object* json_tokener_parse(char *str);
