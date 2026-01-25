use crate::{GridState, gridArea::grid::Grid};

pub mod grid;

pub struct GridArea
{
//XXX sort out this shitty name issue
    grid2: Grid,
}

impl GridArea
{
/*    
    pub fn new(
        cells: &'a Vec<Cell>,
*/
    pub fn new(
        gridState: &GridState,
    ) -> Self
    {   
        let onRightClick = move |cellIndex| {
            //TODO call gridState.removeCell() to delete an item
        };

        let grid = Grid::new(gridState,onRightClick);
        Grid::init(grid);

        Self {
            grid2: grid,
        }
    }
}

