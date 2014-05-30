<?php
$z = yaz_connect("localhost:9999");
yaz_range($z, 1, 2);
yaz_search($z,"rpn", "computer");
yaz_wait();
$error = yaz_error($z);
if (!empty($error)) {
    echo "Error: $error\n";
} else {
    $hits = yaz_hits($z);
    echo "Result count $hits\n";
    for ($p = 1; $p <= 2; $p++) {
        $rec = yaz_record($z, $p, "string");
        if (empty($rec)) break;
        echo "----- $p -----\n$rec";
    }
}
?>
