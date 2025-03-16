
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "console.c" "displays.c" "foxesptemp_main.c" "i2c.c" "lps35hw.c" "network.c" "rg15.c" "scd41.c" "sen50.c" "settings.c" "sgp40.c" "sht4x.c" "ssd130x.c" "submit.c" "webserver.c" "fonts/terminus13norm.c" "fonts/terminus16bold.c" "fonts/terminus38bold.c")
set(COMPONENT_ADD_INCLUDEDIRS "")
set(COMPONENT_EMBED_TXTFILES "web/css.css.min"
                             "web/startpage.html.p00" "web/startpage.html.p01"
                             "web/startpage.js.min"
                             "web/adminmenu.html.p00" "web/adminmenu.html.p01" "web/adminmenu.html.p02"
)

register_component()

# Two commands should be run automatically on every build:
# The first will call 'make' in the web directory to update
# the split webpages if the source file changed.
# The second will update the build number in version.txt,
# so that it increases every time we call 'idf.py build'.
# Should any of these commands cause problems, e.g. due
# to missing dependencies, just comment them out.
add_custom_target(do_always1 ALL
                  WORKING_DIRECTORY "${COMPONENT_DIR}/web"
                  COMMAND "make"
                 )
add_custom_target(do_always2 ALL
                  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                  COMMAND "${CMAKE_SOURCE_DIR}/updateversion.sh"
                 )

# Note that just using
#target_compile_options(${COMPONENT_LIB} PRIVATE -funsigned-char)
# instead will not work, because the gcc devs insist that "'char' and
# 'unsigned char' are always different types even if you tell the compiler
# explicitly through command line option that you want 'char' to be an
# 'unsigned char', and they will not provide any option to suppress
# the gazillion nonsense-warnings that will be caused by this nitpicking".
# So the following is really our only option if we want to keep our
# sanity, and unfortunately it means we might not get warned
# about some _REAL_ problems.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-pointer-sign -Wno-error=pointer-sign)

