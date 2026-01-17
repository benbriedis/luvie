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
badd +118 src/main.rs
badd +80 src/gridArea/grid.rs
badd +32 src/gridArea.rs
badd +174 ~/programming/luvie/testui/grid.cpp
badd +40 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/app/mod.rs
badd +386 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/macros/widget.rs
badd +31 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/widget.rs
badd +352 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/macros/window.rs
badd +1719 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/prelude.rs
badd +1301 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/enums.rs
badd +0 Cargo.toml
argglobal
%argdel
$argadd Cargo.toml
edit src/gridArea/grid.rs
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
exe 'vert 1resize ' . ((&columns * 118 + 92) / 185)
exe 'vert 2resize ' . ((&columns * 66 + 92) / 185)
argglobal
balt ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/prelude.rs
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
let s:l = 80 - ((41 * winheight(0) + 21) / 42)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 80
normal! 011|
wincmd w
argglobal
if bufexists(fnamemodify("~/programming/luvie/testui/grid.cpp", ":p")) | buffer ~/programming/luvie/testui/grid.cpp | else | edit ~/programming/luvie/testui/grid.cpp | endif
if &buftype ==# 'terminal'
  silent file ~/programming/luvie/testui/grid.cpp
endif
balt src/gridArea/grid.rs
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
let s:l = 111 - ((14 * winheight(0) + 21) / 42)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 111
normal! 0
wincmd w
exe 'vert 1resize ' . ((&columns * 118 + 92) / 185)
exe 'vert 2resize ' . ((&columns * 66 + 92) / 185)
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
