#pragma once

inline void* (*Destruct_GetCoreDestructorGroup)();
inline void (*Destruct_AddDestructor)(void* group, void(*func)());
