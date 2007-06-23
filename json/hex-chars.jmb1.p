diff -urw json-c-0.7/json_object.c json-c-0.7-jmb/json_object.c
--- json-c-0.7/json_object.c	2007-03-13 08:25:39.000000000 +0000
+++ json-c-0.7-jmb/json_object.c	2007-06-23 13:33:20.000000000 +0100
@@ -30,7 +30,7 @@
 /* #define REFCOUNT_DEBUG 1 */
 
 char *json_number_chars = "0123456789.+-e";
-char *json_hex_chars = "0123456789abcdef";
+char *json_hex_chars = "0123456789abcdefABCDEF";
 
 #ifdef REFCOUNT_DEBUG
 static char* json_type_name[] = {
