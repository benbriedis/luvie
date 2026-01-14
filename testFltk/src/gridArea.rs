use crate::{Cell, GridSettings, gridArea::grid::Grid};

mod grid;

pub struct GridArea<'a:'static> {
    cells: &'a Vec<Cell>,

//XXX sort out this shitty name issue
    grid2: Grid,
}

impl<'a> GridArea<'a> {
    pub fn new<'b:'static,F1,F2>(
        settings:&'b GridSettings,
        cells: &'b Vec<Cell>,
        addCell: F1,
        modifyCell: F2
    ) -> Self
    where
        F1: Fn(Cell) + 'static,
        F2: Fn(usize,&mut Cell) + 'static,
    {   
        let onRightClick = move |cellIndex| {
            let numCells = cells.len();
            println!("Got right click cellIndex:{cellIndex} numCells:{numCells}");
        };

        let grid = Grid::new(settings, cells,addCell,modifyCell,onRightClick);

        Self {
            cells,
            grid2: grid,
        }
    }
}

