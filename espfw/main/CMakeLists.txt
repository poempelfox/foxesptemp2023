
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "foxesptemp_main.c" "i2c.c" "lps35hw.c" "network.c" "rg15.c" "sen50.c" "settings.c" "sht4x.c" "submit.c" "webserver.c")
set(COMPONENT_ADD_INCLUDEDIRS "")
set(COMPONENT_EMBED_TXTFILES web/css.css.min
                             web/startpage.html.p00 web/startpage.html.p01 web/startpage.html.p02 web/startpage.html.p03
                             web/startpage.js.min
)

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=pointer-sign)

