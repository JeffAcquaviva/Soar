all:
	scons

clean:
	scons -c

distclean: clean
	rm -f config.log
	rm -f .sconsign.dblite
	rm -f SoarSCons.pyc
	rm -rf .sconf_temp
	rm -f SoarLibrary/lib/swt.jar

