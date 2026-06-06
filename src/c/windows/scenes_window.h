#pragma once

// Scene picker for the room currently being controlled. Selecting a scene
// applies it and pops back to the room screen.

void scenes_window_push(void);

// Re-render once the scene list streams in from JS.
void scenes_window_reload(void);
