
all: css.css.min startpage.html.p00 startpage.js.min adminmenu.html.p00

css.css.min: css.css
	minify --output $@ $<

startpage.js.min: startpage.js
	minify --output $@ $<

startpage.html.p00: startpage.html
	rm -f startpage.html.p??
	csplit --quiet --suppress-matched --prefix startpage.html.p $< '/XXXXXXXXXX/' '{*}'

adminmenu.html.p00: adminmenu.html
	rm -f adminmenu.html.p??
	csplit --quiet --suppress-matched --prefix adminmenu.html.p $< '/XXXXXXXXXX/' '{*}'

