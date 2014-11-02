--TEST--
cql
--SKIPIF--
<?php if (!extension_loaded("yaz") || !function_exists("yaz_cql_parse")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("bogus");
if (yaz_cql_parse($z, "computer", $res, false)) {
   echo $res['rpn'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
yaz_cql_conf($z, array(
 "set.cql" => "info:srw/cql-context-set/1/cql-v1.2",
 "index.cql.serverChoice" => "1=1016",
 "relation.eq" => "3=3",
 "structure.*" => "4=1",
 "position.any" => "6=1"
 ));
if (yaz_cql_parse($z, "computer", $res, false)) {
   echo $res['rpn'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
if (yaz_cql_parse($z, "computer and", $res, true)) {
   echo $res['cql'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
if (yaz_cql_parse($z, "computer", $res, true)) {
   echo $res['cql'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
if (yaz_cql_parse($z, "@and a @attr 1=1016 b", $res, true)) {
   echo $res['cql'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
if (yaz_cql_parse($z, "@and a", $res, true)) {
   echo $res['cql'] . "\n";
} else {
   echo $res['errorcode'] . "\n";
}
--EXPECT--
15
@attr 3=3 @attr 4=1 @attr 6=1 @attr 1=1016 "computer"
0
computer
a and b
0
