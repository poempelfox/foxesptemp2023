
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "foxesptemp_main.c" "i2c.c" "lps35hw.c" "network.c" "rg15.c" "scd41.c" "sen50.c" "settings.c" "sht4x.c" "submit.c" "webserver.c")
set(COMPONENT_ADD_INCLUDEDIRS "")
set(COMPONENT_EMBED_TXTFILES "web/css.css.min"
                             "web/startpage.html.p00" "web/startpage.html.p01"
                             "web/startpage.js.min"
                             "web/adminmenu.html.p00" "web/adminmenu.html.p01" "web/adminmenu.html.p02"
)

register_component()

# FIXME: run this automatically always.
#externalproject_add(webstuffmake
#    PREFIX ${COMPONENT_DIR}
#    SOURCE_DIR ${COMPONENT_DIR}/web
#    CONFIGURE_COMMAND ""
#    BUILD_IN_SOURCE 1
#    BUILD_COMMAND make
#    INSTALL_COMMAND ""
#    )

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

