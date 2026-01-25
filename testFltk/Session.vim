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
badd +26 src/gridArea.rs
badd +57 ~/programming/luvie/testFltk/src/gridArea/gridOLD.rs
badd +1 ~/programming/luvie/testFltk/src/customBox.rs
badd +86 ~/programming/luvie/testFltk/src/main.rs
badd +87 ~/programming/luvie/testFltk/src/gridArea/grid.rs
badd +730 ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/macros/widget.rs
argglobal
%argdel
edit ~/programming/luvie/testFltk/src/gridArea/gridOLD.rs
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
exe 'vert 1resize ' . ((&columns * 65 + 92) / 185)
exe 'vert 2resize ' . ((&columns * 119 + 92) / 185)
argglobal
balt ~/programming/luvie/testFltk/src/gridArea/grid.rs
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
let s:l = 57 - ((25 * winheight(0) + 20) / 40)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 57
normal! 07|
wincmd w
argglobal
if bufexists(fnamemodify("~/programming/luvie/testFltk/src/gridArea/grid.rs", ":p")) | buffer ~/programming/luvie/testFltk/src/gridArea/grid.rs | else | edit ~/programming/luvie/testFltk/src/gridArea/grid.rs | endif
if &buftype ==# 'terminal'
  silent file ~/programming/luvie/testFltk/src/gridArea/grid.rs
endif
balt ~/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/fltk-1.5.22/src/macros/widget.rs
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
let s:l = 87 - ((36 * winheight(0) + 20) / 40)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 87
normal! 0
wincmd w
2wincmd w
exe 'vert 1resize ' . ((&columns * 65 + 92) / 185)
exe 'vert 2resize ' . ((&columns * 119 + 92) / 185)
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
