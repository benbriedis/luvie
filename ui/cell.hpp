#ifndef CELL_HPP
#define CELL_HPP

//XXX   Note ==> Cell

typedef struct {
	int   id;          // corresponds to PatternInstance::id in the timeline; 0 if not timeline-backed
	int   row;
	float col;
	float length;
	float startOffset = 0.0f;  // beat offset into the pattern where playback starts
} Note;


#endif

