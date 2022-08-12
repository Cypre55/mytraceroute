run: mytraceroute
	@sudo ./mytraceroute google.com
	
mytraceroute: mytraceroute.c
	@gcc $< -o $@