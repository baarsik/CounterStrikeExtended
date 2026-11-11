# Bot manager replacement

Compiles the actual `InstallBotControl` body with an abstract nonvirtual base
and an owned derived manager. Repeated map replacements must destroy derived
resources before base storage. The original base-only destructor call traps
under Apple Clang 21; complete typed destruction passes. This fixture does not
qualify bot gameplay, navigation, or all manager resources in the real game.
