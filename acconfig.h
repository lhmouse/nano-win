/* $Id$ */

/* Define this if you have the wresize function in your ncurses-type library */
#undef HAVE_WRESIZE

/* Define this if you have the resizeterm function in your ncurses-type library */
#undef HAVE_RESIZETERM

/* Define this if your curses lib has the _use_keypad flag */
#undef HAVE_USEKEYPAD

/* Define this if you have NLS */
#undef ENABLE_NLS

/* Define this is you have the catgets command */
#undef HAVE_CATGETS

/* Define this is you have GNU gettext */
#undef HAVE_GETTEXT

/* Define this for HAVE_LC_MESSAGES */
#undef HAVE_LC_MESSAGES

/* Define this if you have the stpcpy function (cool) */
#undef HAVE_STPCPY

/* Define this to make the nano executable as small as possible */
#undef NANO_SMALL

/* Define to use the slang wrappers for curses instead of native curses */
#undef USE_SLANG

/* Define this to enable the extra stuff */
#undef NANO_EXTRA

/* Define to disable the tab completion code Chris worked so hard on! */
#undef DISABLE_TABCOMP

/* Define this to disable the justify routine */
#undef DISABLE_JUSTIFY

/* Define this to disable the use(full|less) spelling functions */
#undef DISABLE_SPELLER

/* Define this to disable the ^G help menu */
#undef DISABLE_HELP

/* Define this to disable the built-in (crappy) file browser */
#undef DISABLE_BROWSER

/* Define this to disable any and all text wrapping */
#undef DISABLE_WRAPPING

/* Define this to disable the mouse functions */
#undef DISABLE_MOUSE

/* Define this to load files upon inserting them, and allow switching between them; this is disabled if NANO_SMALL is defined */
#undef ENABLE_LOADONINSERT

/* Define this to use the .nanorc file */
#undef ENABLE_NANORC

/* Define this if your curses library has the use_default_colors command */
#undef HAVE_USE_DEFAULT_COLORS

/* Define this to have syntax hilighting, requires ENABLE_NANORC too! */
#undef ENABLE_COLOR

