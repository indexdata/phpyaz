--TEST--
yaz_database
--SKIPIF--
<?php if (!extension_loaded("yaz")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("z3950.indexdata.com");
yaz_database($z, "marc");
yaz_search($z, "rpn", "@attr 1=99 computer");
yaz_wait();
echo yaz_errno($z) . ":" . yaz_error($z) . ":" . yaz_addinfo($z) . "\n";
echo yaz_hits($z) . "\n";
yaz_search($z, "rpn", "computer");
yaz_wait();
echo yaz_errno($z) . ":" . yaz_error($z) . ":" . yaz_addinfo($z) . "\n";
echo yaz_hits($z) . "\n";
--EXPECT--
114:Unsupported Use attribute:99
0
0::
11
