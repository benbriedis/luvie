use crate::{Cell, GridSettings, gridArea::grid::Grid};
use grid::gridImpl::GridImpl;

pub mod grid;

pub struct GridArea<'a> 
{
    cells: &'a Vec<Cell>,

//XXX sort out this shitty name issue
    grid2: Grid,
}

impl<'a:'static> GridArea<'a> 
{
/*    
    pub fn new(
        cells: &'a Vec<Cell>,
*/
    pub fn new<F1,F2>(
        settings:&'a GridSettings,
        cells: &'a Vec<Cell>,
        addCell: F1,
        modifyCell: F2
    ) -> Self
    where
        F1: Fn(Cell) + 'static,
        F2: Fn(usize,Cell) + 'static
    {   
        let onRightClick = move |cellIndex| {
            let numCells = cells.len();
            println!("Got right click cellIndex:{cellIndex} numCells:{numCells}");
        };

        let gridImpl = GridImpl::new(&settings,&cells,addCell,modifyCell,onRightClick);
        let grid = Grid::new(&settings,gridImpl);

        Self {
            cells,
            grid2: grid,
        }
    }
}

