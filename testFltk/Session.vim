let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/programming/luvie/testFltk
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +16 Cargo.toml
badd +8 src/main.rs
badd +108 ~/programming/luvie/testiced/src/grid.rs
badd +147 ~/programming/luvie/testiced/src/main.rs
badd +241 ~/programming/luvie/testui/grid.cpp
badd +27 ~/programming/luvie/testui/main.cpp
badd +56 src/grid.rs
badd +1 src/gridArea.rs
badd +58 ~/programming/luvie/testui/grid.hpp
badd +29 src/customBox.rs
badd +118 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/valuator.rs
badd +597 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/group.rs
badd +24 ~/programming/luvie/testFltk/src/gridArea/grid.rs
argglobal
%argdel
$argadd Cargo.toml
edit src/gridArea.rs
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
1wincmd h
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
wincmd =
argglobal
balt src/customBox.rs
setlocal foldmethod=manual
setlocal foldexpr=0
setlocal foldmarker={{{,}}}
setlocal foldignore=#
setlocal foldlevel=0
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldenable
silent! normal! zE
let &fdl = &fdl
let s:l = 31 - ((30 * winheight(0) + 20) / 40)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 31
normal! 021|
wincmd w
argglobal
if bufexists(fnamemodify("~/programming/luvie/testFltk/src/gridArea/grid.rs", ":p")) | buffer ~/programming/luvie/testFltk/src/gridArea/grid.rs | else | edit ~/programming/luvie/testFltk/src/gridArea/grid.rs | endif
if &buftype ==# 'terminal'
  silent file ~/programming/luvie/testFltk/src/gridArea/grid.rs
endif
balt src/grid.rs
setlocal foldmethod=manual
setlocal foldexpr=0
setlocal foldmarker={{{,}}}
setlocal foldignore=#
setlocal foldlevel=0
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldenable
silent! normal! zE
let &fdl = &fdl
let s:l = 24 - ((23 * winheight(0) + 20) / 40)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 24
normal! 0
wincmd w
2wincmd w
wincmd =
tabnext 1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let &winminheight = s:save_winminheight
let &winminwidth = s:save_winminwidth
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
