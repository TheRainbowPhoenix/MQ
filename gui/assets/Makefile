UNICODES = 0020-00FF;0370-03FF;2070-209F;2190-21FF;2200-22FF;25A0-25FF

all:
	fonttools subset DejaVuSans.ttf --unicodes="$(UNICODES)"
	fonttools subset DejaVuSans-Bold.ttf --unicodes="$(UNICODES)"
	fonttools subset DejaVuSansMono.ttf --unicodes="$(UNICODES)"
