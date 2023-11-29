
all: css.css.min startpage.html.p00 startpage.js.min

css.css.min: css.css
	minify --output $@ $<

startpage.js.min: startpage.js
	minify --output $@ $<

startpage.html.p00: startpage.html
	rm -f startpage.html.p??
	csplit --quiet --suppress-matched --prefix startpage.html.p $< '/XXXXXXXXXX/' '{*}'

