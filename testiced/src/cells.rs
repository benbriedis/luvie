use std::{fmt::Debug};

#[derive(Debug,Default,Clone,Copy,PartialEq)]
pub struct Cell {
    pub row: usize,
    pub col: f32,       //XXX awkward name given type. Might be the best we have for the moment though
    pub length: f32,
    pub velocity: u8
}

/* 
    This is basically a pseudo-ADT for Cells.
    Separating it out helps to provide some componentisation within the Elm architecture.
*/

pub type Cells = Vec<Cell>;

#[derive(Clone, Debug, Copy)]
pub enum CellMessage {
    Add(Cell),
    Modify(usize,Cell),
    Delete(usize)
}

pub fn update(cells: &mut Cells, message: CellMessage) {
    match message {
        CellMessage::Add(cell) => cells.push(cell),
        CellMessage::Modify(i,cell) => cells[i] = cell,
        CellMessage::Delete(i) => { cells.remove(i); }
    }
}

