function(compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    foreach(source ${arg_SOURCES})
        if(${source} MATCHES "\.vert\.glsl$")
            set(stage "vertex")
        elseif(${source} MATCHES "\.frag\.glsl$")
            set(stage "fragment")
        endif()

        string(LENGTH ${source} source_length)
        math(EXPR source_length "${source_length} - 5")
        string(SUBSTRING ${source} 0 ${source_length} source_extensionless)
        add_custom_command(
            OUTPUT ${source_extensionless}.spv
            DEPENDS ${source}
            DEPFILE ${source}.d
            COMMAND ${Vulkan_GLSLC_EXECUTABLE}
                -MD -MF ${source}.d
                -fshader-stage=${stage}
                -o ${source_extensionless}.spv
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        target_sources(${target} PRIVATE ${source_extensionless}.spv)
    endforeach()
endfunction()
