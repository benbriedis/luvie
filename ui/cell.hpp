#ifndef CELL_HPP
#define CELL_HPP

//XXX   Note ==> Cell

typedef struct {
	int   id;      // corresponds to PatternInstance::id in the timeline; 0 if not timeline-backed
	int   row;
	float col;
	float length;
} Note;


#endif

