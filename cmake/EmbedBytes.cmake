# luvie_bytes_to_c(<file> <out-var>)
#
# Read <file> and set <out-var> to its bytes as the body of a C array initialiser --
# "0x89,0x50,..." sixteen to a line, tab-indented -- for embedding in generated source.
# Also registers <file> as a configure dependency, so editing it re-runs configure and
# the new bytes reach the binary.
function(luvie_bytes_to_c file outVar)
    file(READ "${file}" hex HEX)
    # Wrap at 16 bytes a line. Done by slicing the hex string (2 chars a byte, so 32 a
    # line) rather than by regex: CMake's regex engine has no {n} repetition operator,
    # and a "{16}" written there matches literally and silently leaves the whole array
    # on one line, which for a font runs to a few hundred kilobytes.
    string(LENGTH "${hex}" hexLen)
    set(bytes "")
    set(pos 0)
    while(pos LESS hexLen)
        math(EXPR remaining "${hexLen} - ${pos}")
        set(take 32)
        if(remaining LESS 32)
            set(take ${remaining})
        endif()
        string(SUBSTRING "${hex}" ${pos} ${take} line)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," line "${line}")
        string(APPEND bytes "\t${line}\n")
        math(EXPR pos "${pos} + 32")
    endwhile()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
    set(${outVar} "${bytes}" PARENT_SCOPE)
endfunction()
