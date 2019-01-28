function calcAdditional(entry, cell) {
    cell.halite_plus_ship = cell.halite;
    cell.enemy_ship_there = false;
    if(cell.ship_id != -1) {
        var s = entry.data.ships[cell.ship_id];
        cell.halite_plus_ship += s.halite;
        cell.enemy_ship_there = !s.ally;
    }
}

function test(entry) {
    console.log("-------------" + entry.id + "-------------");
    
    var current_cell = entry.data.map[entry.data.current_cell[1]][entry.data.current_cell[0]];
    var moving_cell = entry.data.map[entry.data.moving_cell[1]][entry.data.moving_cell[0]];
    
    calcAdditional(entry, current_cell);
    calcAdditional(entry, moving_cell);
    
    var can_move = false;
    can_move |= moving_cell.friendliness > -1;
    //can_move |= moving_cell.friendliness > -11 && moving_cell.halite > 400;
    if(moving_cell.enemy_ship_there) {
        if(moving_cell.friendliness < 7 && current_cell.halite_plus_ship > moving_cell.halite_plus_ship) {
            can_move = false;
        }
    }
    
    return can_move ? 'good' : 'bad';
}