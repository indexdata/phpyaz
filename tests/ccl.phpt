--TEST--
yaz_ccl_parse
--SKIPIF--
<?php if (!extension_loaded("yaz")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("z3950.indexdata.com/gils");
yaz_ccl_conf($z, array(
 "ti" => "u=4 s=pw t=l,r",
 "term" => "1=1016 s=al,pw t=z",
 ));
if (yaz_ccl_parse($z, "a", $res)) {
   echo "rpn=" . $res['rpn'] . "\n";
} else {
   echo "errorcode=" . $res['errorcode'] . "\n";
   echo "errorstring=" . $res['errorstring'] . "\n";
   echo "errorpos=" . $res['errorpos'] . "\n";
}
if (yaz_ccl_parse($z, "unknown=a", $res)) {
   echo "rpn=" . $res['rpn'] . "\n";
} else {
   echo "errorcode=" . $res['errorcode'] . "\n";
   echo "errorstring=" . $res['errorstring'] . "\n";
   echo "errorpos=" . $res['errorpos'] . "\n";
}
if (yaz_ccl_parse($z, "ti=computer", $res)) {
   echo "rpn=" . $res['rpn'] . "\n";
} else {
   echo "errorcode=" . $res['errorcode'] . "\n";
   echo "errorstring=" . $res['errorstring'] . "\n";
   echo "errorpos=" . $res['errorpos'] . "\n";
}
--EXPECT--
rpn=@attr 4=2 @attr 1=1016 a 
errorcode=6
errorstring=Unknown qualifier
errorpos=0
rpn=@attr 4=2 @attr 1=4 computer
