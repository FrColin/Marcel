/* Lua.h
 * 	Shared definition
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */

#ifndef LUA_H
#define LUA_H

#ifdef LUA

extern void init_Lua(const char *luascript);
extern int findUserFunc( const char *fn );
extern void execUserFuncDeadPublisher( const char *name, int funcid, const char *, const char *);
extern void execUserFuncEvery( const char *name,  int funcid );
#endif
#endif

