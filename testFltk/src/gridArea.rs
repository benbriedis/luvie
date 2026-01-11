use crate::{Cell, GridSettings, gridArea::grid::Grid};

mod grid;


pub fn createGridArea<'a:'static>(settings:&'a GridSettings, cells: &'a Vec<Cell>) 
{
    //TODO add the context menu

    Grid::new(settings, cells);
}
