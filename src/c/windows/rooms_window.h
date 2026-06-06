#pragma once

// Home screen: the list of rooms plus an "All Off" quick action. When the
// bridge isn't paired yet it shows a single row that kicks off pairing.

void rooms_window_push(void);

// Re-render after the model changes (room list arrived, status changed).
void rooms_window_reload(void);
