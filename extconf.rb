require 'mkmf-rice'

abort 'No boost_system' unless have_library('boost_system')
abort 'No libstdc++' unless have_library('stdc++')
abort 'No boost_iostreams' unless have_library('boost_iostreams')
abort 'No zlib' unless have_library('z')

# TODO: Checking for websocketpp and/or moving it into our local
#       headers since it's header only anyway.

# Add our local include directory to the include path
$INCFLAGS << ' -I$(srcdir)/include'

create_makefile 'discordrb'
