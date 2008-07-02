
ADD_CUSTOM_TARGET(implementation_doc
        ALL
        COMMAND ${ASCIIDOC_EXECUTABLE} ${ASCIIDOC_OPTIONS} Implementation_Guide.txt
        DEPENDS Implementation_Guide.txt
        WORKING_DIRECTORY
        COMMENT "Building implementation documentation")

