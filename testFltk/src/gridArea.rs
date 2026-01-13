use crate::{Cell, GridSettings, gridArea::grid::Grid};

mod grid;


pub fn createGridArea<'a:'static>(settings:&'a GridSettings, cells: &'a Vec<Cell>,
    addCell: &fn(&Cell),
    modifyCell: &fn(cellIndex:usize,&mut Cell),
) 
{
    //TODO add the context menu

    let rightClick = |cellIndex:usize| {
        println!("Got right click");
    };

    Grid::new(settings, cells,addCell,modifyCell,&rightClick);
}
