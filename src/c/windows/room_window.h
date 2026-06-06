#pragma once

// Single-room control: toggle, brightness ramp (Up/Down), and a long-press
// on Select to open that room's scenes.

void room_window_push(int room_index);

// Called when a fresh room list arrives so an open control screen can refresh.
void room_window_update(void);
