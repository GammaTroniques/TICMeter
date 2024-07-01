
execute_process(COMMAND git log --pretty=format:'%h' -n 1
                OUTPUT_VARIABLE GIT_REV
                ERROR_QUIET)
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else()
    execute_process(
        COMMAND bash -c "git diff --quiet --exit-code || echo +"
        OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
        COMMAND git describe --exact-match --tags
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 7 GIT_REV)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
endif()

set(VERSION "
#ifndef VERSION_H
#define VERSION_H

#define GIT_REV \"${GIT_REV}${GIT_DIFF}\"
#define GIT_BRANCH \"${GIT_BRANCH}\"
#define BUILD_TIME \"${BUILD_TIME}\"

")

if ("${GIT_TAG}" STREQUAL "")
else()
    # append GIT_TAG to VERSION
set(VERSION "${VERSION} 
#define GIT_TAG \"${GIT_TAG}\"
")
endif()

set(VERSION "${VERSION} 
#endif // VERSION_H
")


if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version.h)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.h VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.h "${VERSION}")
endif()

# version.txt
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt)
#     file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt "${GIT_TAG}")
# endif()


idf_component_register(SRCS
                       SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       INCLUDE_DIRS "."
                    #    EMBED_TXTFILES ${project_dir}/main/certs/ca_cert.pem
                       )
 

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)