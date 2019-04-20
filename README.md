# Discordrb CPP

### An extremely unnessecary proof of concept.

This is an implementation of a Discord gateway using websocketpp, and tightly integrated with Ruby native
extensions. It is intended to serve as somewhat of a drop in replacement for discordrb's gateway.

More than anything this is a proof of concept, and holds little to no real world advantage over the existing websocket
implementation. However, it does show improved parsing times, and has thusfar not shown to have the same bugs as the
ruby implementation (which is good, because heartbeat events are not raised, making health checks tricky).

# Building

You must have websocketpp, libstdc++, boost, boost_iostreams, boost_system, and zlib libraries installed to properly run
the extension.

You can generate the makefile with

```sh
ruby extconf.rb
```

And then you can build with

```sh
make
```

Any warnings about command line options can be ignored, as they are results of invalid options in the rice-mkmf generator.

---

# Usage

To start using the extension you should follow `example.rb` in the top directory. Since not all features are implemented,
the Bot class needs to be patched to remove usage of unimplemented methods.

# Thanks

Many thanks to [zeroxs](https://github.com/zeroxs/aegis.cpp) whose C++ library enabled me to figure out certain aspects when
I was clueless.
