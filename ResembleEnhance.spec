# -*- mode: python ; coding: utf-8 -*-

block_cipher = None

# We use a single folder for the portable version to keep it organized
# The EXE will be in the root, and we'll have bin/ and models/ alongside it.

from PyInstaller.utils.hooks import collect_all

datas_re, binaries_re, hidden_re = collect_all('resemble_enhance')
datas_moviepy, binaries_moviepy, hidden_moviepy = collect_all('moviepy')
datas_imageio, binaries_imageio, hidden_imageio = collect_all('imageio')
datas_imageio_ffmpeg, binaries_imageio_ffmpeg, hidden_imageio_ffmpeg = collect_all('imageio_ffmpeg')
datas_proglog, binaries_proglog, hidden_proglog = collect_all('proglog')
datas_decorator, binaries_decorator, hidden_decorator = collect_all('decorator')
datas_ctk, binaries_ctk, hidden_ctk = collect_all('customtkinter')

a = Analysis(
    ['src/main_gui.py'],
    pathex=['src'],
    binaries=binaries_re + binaries_moviepy + binaries_imageio + binaries_imageio_ffmpeg + binaries_proglog + binaries_decorator + binaries_ctk,
    datas=[
        ('docs/仕様書.md', 'docs'),
        ('docs/specs.md', 'docs'),
        ('assets/icon.png', 'assets'),
    ] + datas_re + datas_moviepy + datas_imageio + datas_imageio_ffmpeg + datas_proglog + datas_decorator + datas_ctk,
    hiddenimports=[
        'resemble_enhance',
        'resemble_enhance.enhancer',
        'resemble_enhance.enhancer.inference',
        'resemble_enhance.denoiser',
        'resemble_enhance.denoiser.inference',
        'customtkinter',
        'tkinterdnd2',
        'torch',
        'torchaudio',
        'pydantic',
        'moviepy',
        'moviepy.video.io.VideoFileClip',
        'moviepy.audio.io.AudioFileClip',
    ] + hidden_re + hidden_moviepy + hidden_imageio + hidden_imageio_ffmpeg + hidden_proglog + hidden_decorator + hidden_ctk,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='ResembleEnhance',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False, # Set to True for debugging
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['assets/icon.png'], 
)
coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='ResembleEnhancePortable',
)
