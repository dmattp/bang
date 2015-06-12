Process {
    echo "Testing $($_.name)"
    .\bang.exe $($_ | select -expand fullname) | out-file -enc ascii $('.\test-ref\' + $_.name + '.out')
}