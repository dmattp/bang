Process {
    echo "TESTING $($_.name)"
#    read-host
    $t = .\Release\bang $_.fullname
    $ref = cat .\test-ref\$($_.name + '.out')
    if (compare-object $t $ref) {
        throw "BAD TEST $($_.name)"
    }
}