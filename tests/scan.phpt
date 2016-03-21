--TEST--
yaz_connect
--SKIPIF--
<?php if (!extension_loaded("yaz")) print "skip"; ?>
--FILE--
<?php
$z = yaz_connect("z3950.indexdata.com/marc");
yaz_scan($z, "rpn", "@attr 1=4 a", array("number" => 4));
yaz_wait();
$entries = yaz_scan_result($z, $scanr);
print_r($scanr);
print_r($entries);
--EXPECT--
Array
(
    [number] => 4
    [stepsize] => 0
    [position] => 1
    [status] => 0
)
Array
(
    [0] => Array
        (
            [0] => term
            [1] => a
            [2] => 7
            [3] => a
        )

    [1] => Array
        (
            [0] => term
            [1] => access
            [2] => 1
            [3] => access
        )

    [2] => Array
        (
            [0] => term
            [1] => action
            [2] => 1
            [3] => action
        )

    [3] => Array
        (
            [0] => term
            [1] => adam
            [2] => 3
            [3] => Adam
        )

)

