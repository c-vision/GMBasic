// FILE: Basic.Runtime.CGfxGraphicsService.cs
// Bridge C# -> C del motore grafico unico (libgmb_gfx.dylib).
//
// REGOLE:
//  - C# chiama il C (P/Invoke); MAI C -> C#.
//  - SDL2 richiede il MAIN THREAD: init finestra/present/poll eventi passano SOLO da
//    MainThreadStart/MainThreadTick/MainThreadShouldQuit/MainThreadStop (chiamati dal
//    runner sul main thread).
//  - I metodi del servizio invocati dal thread del programma BASIC vengono ACCODATI e
//    eseguiti dal main thread dentro MainThreadTick (niente SDL da thread secondari).
//    I metodi con valore di ritorno fanno un round-trip con attesa breve.
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;

namespace Basic.Runtime;

public sealed class CGfxGraphicsService : IGraphicsService
{
    private const string Lib = "gmb_gfx";
    private const int RgbFlag = 0x40000000;

    // ── P/Invoke (motore C) ──
    [DllImport(Lib)] private static extern void gmb_gfx_init();
    [DllImport(Lib)] private static extern void gmb_gfx_screen(int mode, int w, int h, int bpp);
    [DllImport(Lib)] private static extern void gmb_gfx_set_coord_mode(int mode);
    [DllImport(Lib)] private static extern void gmb_gfx_console_select(int mode);
    [DllImport(Lib)] private static extern void gmb_gfx_screenset(int visible, int active);
    [DllImport(Lib)] private static extern void gmb_gfx_pcopy(int src, int dst);
    [DllImport(Lib)] private static extern void gmb_gfx_color(int fg, int bg);
    [DllImport(Lib)] private static extern void gmb_gfx_palette(int index, int r, int g, int b);
    [DllImport(Lib)] private static extern void gmb_gfx_cls(int color);
    [DllImport(Lib)] private static extern void gmb_gfx_pset(int x, int y, int color);
    [DllImport(Lib)] private static extern void gmb_gfx_preset(int x, int y);
    [DllImport(Lib)] private static extern int gmb_gfx_point(int x, int y);
    [DllImport(Lib)] private static extern double gmb_gfx_canvas_op(string op, double[] args, int nargs, string? path);
    [DllImport(Lib)] private static extern void gmb_gfx_line(int x1, int y1, int x2, int y2, int color, string? style);
    [DllImport(Lib)] private static extern void gmb_gfx_circle(int x, int y, int r, int color, int fill, double start, double end, double aspect);
    [DllImport(Lib)] private static extern void gmb_gfx_paint(int x, int y, int fill, int border);
    [DllImport(Lib)] private static extern void gmb_gfx_draw(string command);
    [DllImport(Lib)] private static extern void gmb_gfx_polygon(float[] points, int count, int color);
    [DllImport(Lib)] private static extern void gmb_gfx_polygon_fill(float[] points, int count, int color);
    [DllImport(Lib)] private static extern void gmb_gfx_font(string path, int size);
    [DllImport(Lib)] private static extern void gmb_gfx_locate(int x, int y);
    [DllImport(Lib)] private static extern void gmb_gfx_print(string text);
    [DllImport(Lib)] private static extern void gmb_gfx_draw_text(string text, int x, int y, int align, int color);
    [DllImport(Lib)] private static extern void gmb_gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, uint rgba);
    [DllImport(Lib)] private static extern void gmb_gfx_stroke_rounded_rect(int x, int y, int w, int h, int radius, uint rgba);
    [DllImport(Lib)] private static extern void gmb_gfx_fill_circle(int cx, int cy, int d, uint rgba);
    [DllImport(Lib)] private static extern int gmb_gfx_text_width(string text);
    [DllImport(Lib)] private static extern int gmb_gfx_text_height();
    [DllImport(Lib)] private static extern int gmb_gfx_text_width_logical(string text);
    [DllImport(Lib)] private static extern int gmb_gfx_text_height_logical();
    [DllImport(Lib)] private static extern void gmb_gfx_sync();
    [DllImport(Lib)] private static extern void gmb_gfx_window_set_visible(int on);
    [DllImport(Lib)] private static extern int gmb_gfx_save_screen_png(string path);
    [DllImport(Lib)] private static extern void gmb_gfx_view(int x1, int y1, int x2, int y2);
    [DllImport(Lib)] private static extern void gmb_gfx_window(int x1, int y1, int x2, int y2);
    [DllImport(Lib)] private static extern void gmb_gfx_bsave(int x1, int y1, int x2, int y2);
    [DllImport(Lib)] private static extern void gmb_gfx_bload(int x, int y);
    [DllImport(Lib)] private static extern int gmb_gfx_image_get(int x1, int y1, int x2, int y2, int[] outv, int max);
    [DllImport(Lib)] private static extern void gmb_gfx_image_put(int x, int y, int[] img, int n);
    [DllImport(Lib)] private static extern void gmb_gfx_image_puts_n(int[] sprites, int spritesLen, float[] positions, int count);
    [DllImport(Lib)] private static extern int gmb_gfx_load_texture(string path);
    [DllImport(Lib)] private static extern void gmb_gfx_draw_texture(int id, int x, int y, double rot, double sx, double sy);
    [DllImport(Lib)] private static extern void gmb_gfx_draw_texture_frame(int id, int frame, int x, int y);
    [DllImport(Lib)] private static extern void gmb_gfx_draw_pixel_buffer(int w, int h, uint[] pixels);
    [DllImport(Lib)] private static extern int gmb_gfx_sprite_load(string path);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_position(int id, float x, float y);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_visible(int id, int visible);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_priority(int id, int priority);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_scale(int id, double sx, double sy);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_rotation(int id, double degrees);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_draw(int id);
    [DllImport(Lib)] private static extern int gmb_gfx_sprite_pixels_to_array(int src, int[] outv, int max);
    [DllImport(Lib)] private static extern int gmb_gfx_collide(int id1, int id2);
    [DllImport(Lib)] private static extern int gmb_gfx_take_collisions(int[] id1, int[] id2, float[] x, float[] y, int max);
    [DllImport(Lib)] private static extern int gmb_gfx_chart_create();
    [DllImport(Lib)] private static extern void gmb_gfx_chart_plot(int handle, double[] xs, double[] ys, int count, string label);
    [DllImport(Lib)] private static extern void gmb_gfx_chart_show(int handle, int x, int y);
    [DllImport(Lib)] private static extern void gmb_gfx_chart_clear(int handle);
    [DllImport(Lib)] private static extern void gmb_gfx_chart_close(int handle);
    [DllImport(Lib)] private static extern int gmb_gfx_chart_save(int handle, string path);
    [DllImport(Lib)] private static extern void gmb_gfx_clear_rect(int x1, int y1, int x2, int y2, int color);
    [DllImport(Lib)] private static extern int gmb_gfx_restore_screen_png(string path);
    [DllImport(Lib)] private static extern int gmb_gfx_sprite_def(int id, int w, int h, int[] pixels);
    [DllImport(Lib)] private static extern void gmb_gfx_sprite_set_matrix(int id, double[] m6);
    [DllImport(Lib)] private static extern int gmb_gfx_tilemap_from_texture(int textureId, int cellW, int cellH, int cols, int rows);
    [DllImport(Lib)] private static extern int gmb_gfx_tilemap_offset_x(int handle);
    [DllImport(Lib)] private static extern int gmb_gfx_tilemap_offset_y(int handle);
    [DllImport(Lib)] private static extern void gmb_gfx_render_cells(int[] chars, uint[] fg, uint[] bg, int w, int h);
    [DllImport(Lib)] private static extern void gmb_gfx_set_cursor_state(int visible);
    [DllImport(Lib)] private static extern void gmb_gfx_set_cursor_shape(int shape);
    [DllImport(Lib)] private static extern void gmb_gfx_set_attribute(int attr);
    [DllImport(Lib)] private static extern int gmb_gfx_get_cursor_x();
    [DllImport(Lib)] private static extern int gmb_gfx_get_cursor_y();
    [DllImport(Lib)] private static extern void gmb_gfx_tab_zone();
    [DllImport(Lib)] private static extern int gmb_gfx_sprite_count();
    [DllImport(Lib)] private static extern void gmb_gfx_unload_texture(int id);
    [DllImport(Lib)] private static extern int gmb_gfx_screen_width();
    [DllImport(Lib)] private static extern int gmb_gfx_screen_height();
    [DllImport(Lib)] private static extern void gmb_gfx_chart_plot_financial(int handle, double[] ohlc, int count, string plotType, string label);
    [DllImport(Lib)] private static extern int gmb_gfx_load_texture_sheet(string path, int cellW, int cellH);
    [DllImport(Lib)] private static extern void gmb_gfx_tilemap_set(int handle, int col, int row, int tile);
    [DllImport(Lib)] private static extern void gmb_gfx_tilemap_offset(int handle, int dx, int dy);
    [DllImport(Lib)] private static extern void gmb_gfx_tilemap_scroll(int handle, int dx, int dy);
    [DllImport(Lib)] private static extern void gmb_gfx_tilemap_draw(int handle, int offsetX, int offsetY);


    // ── Form/controlli pixel (bridge C#→C; per le gambe gestite) ──
    [DllImport(Lib)] private static extern void gmb_form_reset();
    [DllImport(Lib)] private static extern void gmb_form_frame(int frameMs);
    [DllImport(Lib)] private static extern void gmb_ui_frame(int frameMs);
    [DllImport(Lib)] private static extern void gmb_window_resize(int w, int h);
    [DllImport(Lib)] private static extern void gmb_tui_add_checkbox(int x, int y, int w, string label);
    [DllImport(Lib)] private static extern int gmb_tui_last_handle();
    [DllImport(Lib)] private static extern void gmb_control_apply_props_handle(int handle, uint flags, string name, string tag, string tooltip, int tabindex, int visible, int enabled, int tabstop);
    [DllImport(Lib)] private static extern void gmb_control_apply_style_handle(int handle, string spec);
    /* Union-aware: applica lo stile al registry GIUSTO (form widget/button o TUI) guardando
     * l'HANDLE. `gmb_control_apply_style_handle` cerca solo nel registry TUI: su un BUTTON
     * pixel (form) le clausole STYLE "grow=… minw=… maxw=…" si perdevano su IL/PCode (VM/C
     * usa già la versione che copre entrambi). */
    [DllImport(Lib)] private static extern void gmb_form_apply_style_handle(int handle, string spec);
    [DllImport(Lib)] private static extern void gmb_tui_controls_reset();
    [DllImport(Lib)] private static extern void gmb_tui_add_tbrowse(int row, int col, int w, int h, string colsCsv, string dataCsv);
    [DllImport(Lib)] private static extern void gmb_tui_add_filedialog(string title, string mask);
    [DllImport(Lib)] private static extern void gmb_tui_add_printerdialog(string title);
    [DllImport(Lib)] private static extern void gmb_tui_add_treeview(int x, int y, int w, int h, string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_grid(int x,int y,int w,int h,int cols,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_datepicker(int x,int y,int w);
    [DllImport(Lib)] private static extern void gmb_tui_add_colorpicker(int x,int y,int w,int h);
    [DllImport(Lib)] private static extern void gmb_tui_add_listbox(int x,int y,int w,int h,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_combobox(int x,int y,int w,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_radiogroup(int x,int y,int w,int h,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_toggle(int x,int y,int w,string caption);
    [DllImport(Lib)] private static extern void gmb_tui_add_slider(int x,int y,int w,int mn,int mx,int val);
    [DllImport(Lib)] private static extern void gmb_tui_add_numericupdown(int x,int y,int w,int mn,int mx,int val);
    [DllImport(Lib)] private static extern void gmb_tui_add_starrating(int x,int y,int w,int mx,int val);
    [DllImport(Lib)] private static extern void gmb_tui_add_scrollbar(int x,int y,int len,int mn,int mx,int val);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_scrollview(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_label(int x,int y,int w,string text);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_datalist(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_propertygrid(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_splitter(int x,int y,int w,int h,int orient);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_contextmenu(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_logview(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_hexview(int x,int y,int w,int h,string data);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_bigtext(int x,int y,string text);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_codearea(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_reorderlistbox(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_alertbox(int x,int y,int w,int h,string text);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_confirmationbox(int x,int y,int w,int h,string text);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_fileexplorer(int x,int y,int w,int h,string path);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_tui_add_dashboard(int x,int y,int w,int h,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_spinner(int x,int y,int w,string preset);
    [DllImport(Lib)] private static extern void gmb_tui_add_statusbar(int x,int y,int w,string text);
    [DllImport(Lib)] private static extern void gmb_tui_add_fkeybar(int x,int y,int w,string keys);
    [DllImport(Lib)] private static extern void gmb_form_dispatch_mouse_px(int type,int x,int y,int button,int wheel);
    [DllImport(Lib)] private static extern void gmb_form_dispatch_key(string key);
    [DllImport(Lib)] private static extern void gmb_tui_add_candlestick(int x,int y,int w,int h,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_chart(int x,int y,int w,int h,string csv);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern void gmb_gfx_chart_set(int handle,string prop,double num,string str);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern double gmb_gfx_chart_get(int handle,string prop);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern IntPtr gmb_gfx_chart_get_str(int handle,string prop);
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern double gmb_gfx_chart_draw(int handle,string op,double[] args,int nargs);
    [DllImport(Lib)] private static extern void gmb_tui_add_tab(int x,int y,int w,string csv);
    [DllImport(Lib)] private static extern void gmb_tui_add_menu(int x,int y,int w,string csv);
    [DllImport(Lib)] private static extern int  gmb_form_closed();
    [DllImport(Lib)] private static extern void gmb_form_request_close();
    [DllImport(Lib)] private static extern void gmb_form_set_last_style(string spec);
    [DllImport(Lib)] private static extern double gmb_form_control_get(string name, string prop);
    [DllImport(Lib)] private static extern void gmb_form_control_set(string name, string prop, double num, string str);
    [DllImport(Lib)] private static extern IntPtr gmb_form_control_get_str(string name, string prop);
    [DllImport(Lib)] private static extern void gmb_form_control_move(string name, int x, int y, int w, int h);
    [DllImport(Lib)] private static extern void gmb_form_control_focus(string name);
    [DllImport(Lib)] private static extern void gmb_form_control_zorder(string name, int front);
    [DllImport(Lib)] private static extern void gmb_form_control_invalidate(string name);
    [DllImport(Lib)] private static extern void gmb_form_control_refresh(string name);
    [DllImport(Lib)] private static extern int gmb_form_last_handle();
    [DllImport(Lib)] private static extern int gmb_form_firing_control();
    [DllImport(Lib)] private static extern int gmb_form_firing_event();
    [DllImport(Lib)] private static extern void gmb_form_fire_event(int handle, int ev);
    [DllImport(Lib)] private static extern void gmb_form_set_last_event(int handle, int ev);
    [DllImport(Lib)] private static extern int gmb_form_ev_index(string name);
    [DllImport(Lib, EntryPoint = "gmb_form_set_event_id")] private static extern void gmb_form_set_event_id_ptr(int handle, int ev, IntPtr cb);
    [DllImport(Lib)] private static extern double gmb_form_clicked(int handle);
    [DllImport(Lib)] private static extern double gmb_form_dblclicked(int handle);
    [DllImport(Lib)] private static extern double gmb_form_changed(int handle);
    [DllImport(Lib)] private static extern double gmb_form_gotfocused(int handle);
    [DllImport(Lib)] private static extern double gmb_form_lostfocused(int handle);
    [DllImport(Lib)] private static extern double gmb_mouse_clicked();
    [DllImport(Lib)] private static extern double gmb_mouse_dblclicked();
    [DllImport(Lib)] private static extern double gmb_mouse_pressed();
    [DllImport(Lib)] private static extern double gmb_key_pressed();
    [DllImport(Lib)] private static extern IntPtr gmb_dt_dtoc_cstr(double oa);

    /// <summary>DTOC(oa) via runtime C (`gmb_dt_dtoc_cstr`) — canon IL/PCode/VM-C ("mm-dd-yyyy"),
    /// eliminando la dipendenza da ClipperLib C# (ClipperDates.Dtoc).</summary>
    public string Dtoc(double oa) { try { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gmb_dt_dtoc_cstr(oa)) ?? ""; } catch { return ""; } }

    [DllImport(Lib)] private static extern int gmb_picture_apply_number(double value, string picture, System.Text.StringBuilder outBuf, int outCap);
    [DllImport(Lib)] private static extern int gmb_picture_apply_text(string value, string picture, System.Text.StringBuilder outBuf, int outCap);
    /// <summary>FormatPicture: applica un PICTURE a un numero/stringa via runtime C
    /// (`gmb_picture_apply_number/text`) — elimina ClipperLib.FormatPicture.</summary>
    public string PictureApply(object? value, string picture)
    {
        try
        {
            switch (value)
            {
                case string s: return PictureApplyText(s, picture);
                case bool b: return b ? "T" : "F";
                case double d: return PictureApplyNumber(d, picture);
                case int i: return PictureApplyNumber(i, picture);
                case long l: return PictureApplyNumber(l, picture);
                default: return PictureApplyText(value?.ToString() ?? "", picture);
            }
        }
        catch { return value?.ToString() ?? ""; }
    }
    private string PictureApplyNumber(double v, string pic) { var sb = new System.Text.StringBuilder(256); try { gmb_picture_apply_number(v, pic ?? "", sb, 256); } catch { } return sb.ToString(); }
    private string PictureApplyText(string v, string pic) { var sb = new System.Text.StringBuilder(256); try { gmb_picture_apply_text(v ?? "", pic ?? "", sb, 256); } catch { } return sb.ToString(); }

    // ---- Clipper printer (stato C condiviso: gmb_clipper_global_state) ----
    [DllImport(Lib)] private static extern IntPtr gmb_clipper_global_state();
    [DllImport(Lib)] private static extern void gmb_clipper_printer_write(IntPtr st, string s, bool newLine);
    [DllImport(Lib)] private static extern int gmb_clipper_printer_only(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_printer_routing(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_printer_prow(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_printer_pcol(IntPtr st);
    [DllImport(Lib)] private static extern IntPtr gmb_clipper_printer_text(IntPtr st);
    [DllImport(Lib)] private static extern void gmb_clipper_printer_eject(IntPtr st);
    public void ClipperPrinterWrite(string s, bool newLine) { try { gmb_clipper_printer_write(gmb_clipper_global_state(), s ?? "", newLine); } catch { } }
    public bool ClipperPrinterOnly() { try { return gmb_clipper_printer_only(gmb_clipper_global_state()) != 0; } catch { return false; } }
    public bool ClipperPrinterRouting() { try { return gmb_clipper_printer_routing(gmb_clipper_global_state()) != 0; } catch { return false; } }
    public int ClipperPrinterProw() { try { return gmb_clipper_printer_prow(gmb_clipper_global_state()); } catch { return 0; } }
    public int ClipperPrinterPcol() { try { return gmb_clipper_printer_pcol(gmb_clipper_global_state()); } catch { return 0; } }
    public void ClipperPrinterEject() { try { gmb_clipper_printer_eject(gmb_clipper_global_state()); } catch { } }
    public string ClipperPrinterText() { try { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gmb_clipper_printer_text(gmb_clipper_global_state())) ?? ""; } catch { return ""; } }
    [DllImport(Lib)] private static extern int gmb_clipper_set_printer(IntPtr st, int on);
    [DllImport(Lib)] private static extern IntPtr gmb_clipper_set_device(IntPtr st, string device);
    public void ClipperSetPrinter(bool on) { try { gmb_clipper_set_printer(gmb_clipper_global_state(), on ? 1 : 0); } catch { } }
    public void ClipperSetDevice(string device) { try { gmb_clipper_set_device(gmb_clipper_global_state(), device ?? ""); } catch { } }
    // ---- Clipper SET settings (stato C condiviso) ----
    [DllImport(Lib)] private static extern int gmb_clipper_set_wrap(IntPtr st, int on);
    [DllImport(Lib)] private static extern int gmb_clipper_set_escape(IntPtr st, int on);
    [DllImport(Lib)] private static extern int gmb_clipper_set_intensity(IntPtr st, int on);
    [DllImport(Lib)] private static extern int gmb_clipper_set_epoch(IntPtr st, int year);
    [DllImport(Lib)] private static extern int gmb_clipper_set_decimals(IntPtr st, int n);
    [DllImport(Lib)] private static extern int gmb_clipper_set_fixed(IntPtr st, int on);
    [DllImport(Lib)] private static extern int gmb_clipper_set_typeahead(IntPtr st, int size);
    [DllImport(Lib)] private static extern IntPtr gmb_clipper_set_delimiters(IntPtr st, string value);
    [DllImport(Lib)] private static extern IntPtr gmb_clipper_set_date_format(IntPtr st, string fmt);
    [DllImport(Lib)] private static extern int gmb_clipper_set_message(IntPtr st, int row);
    [DllImport(Lib)] private static extern int gmb_clipper_set_format_number(IntPtr st, double d, System.Text.StringBuilder outBuf, int cap);
    public void ClipperSetWrap(bool on) { try { gmb_clipper_set_wrap(gmb_clipper_global_state(), on ? 1 : 0); } catch { } }
    public void ClipperSetEscape(bool on) { try { gmb_clipper_set_escape(gmb_clipper_global_state(), on ? 1 : 0); } catch { } }
    public void ClipperSetIntensity(bool on) { try { gmb_clipper_set_intensity(gmb_clipper_global_state(), on ? 1 : 0); } catch { } }
    public void ClipperSetEpoch(int year) { try { gmb_clipper_set_epoch(gmb_clipper_global_state(), year); } catch { } }
    public int ClipperEpoch() { try { return gmb_clipper_set_epoch(gmb_clipper_global_state(), -1); } catch { return 1900; } }
    [DllImport(Lib)] private static extern void gmb_ui_set_theme_name(string name);
    [DllImport(Lib)] private static extern IntPtr gmb_ui_theme_name();
    public void ClipperSetThemeName(string name) { try { gmb_ui_set_theme_name(string.IsNullOrEmpty(name) ? "turbovision" : name); } catch { } }
    public string ClipperThemeName() { try { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gmb_ui_theme_name()) ?? "Turbo"; } catch { return "Turbo"; } }
    [DllImport(Lib)] private static extern void gmb_clipper_set_fg_rgb(IntPtr st, int r, int g, int b, int isDefault);
    [DllImport(Lib)] private static extern void gmb_clipper_set_bg_rgb(IntPtr st, int r, int g, int b, int isDefault);
    [DllImport(Lib)] private static extern int gmb_clipper_get_fg_packed(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_get_bg_packed(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_get_fg_default(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_get_bg_default(IntPtr st);
    public void ClipperSetFgBg(CommonLib.Screens.CellColor fg, CommonLib.Screens.CellColor bg)
    {
        try
        {
            var st = gmb_clipper_global_state();
            gmb_clipper_set_fg_rgb(st, fg.R, fg.G, fg.B, fg.IsDefault ? 1 : 0);
            gmb_clipper_set_bg_rgb(st, bg.R, bg.G, bg.B, bg.IsDefault ? 1 : 0);
        }
        catch { }
    }
    public CommonLib.Screens.CellColor ClipperFg()
    {
        try
        {
            var st = gmb_clipper_global_state();
            int p = gmb_clipper_get_fg_packed(st);
            return new CommonLib.Screens.CellColor { R = (byte)((p >> 16) & 0xFF), G = (byte)((p >> 8) & 0xFF), B = (byte)(p & 0xFF), IsDefault = gmb_clipper_get_fg_default(st) != 0 };
        }
        catch { return CommonLib.Screens.CellColor.White; }
    }
    public CommonLib.Screens.CellColor ClipperBg()
    {
        try
        {
            var st = gmb_clipper_global_state();
            int p = gmb_clipper_get_bg_packed(st);
            return new CommonLib.Screens.CellColor { R = (byte)((p >> 16) & 0xFF), G = (byte)((p >> 8) & 0xFF), B = (byte)(p & 0xFF), IsDefault = gmb_clipper_get_bg_default(st) != 0 };
        }
        catch { return CommonLib.Screens.CellColor.Black; }
    }
    public void ClipperSetDecimals(int n) { try { gmb_clipper_set_decimals(gmb_clipper_global_state(), n); } catch { } }
    public void ClipperSetFixed(bool on) { try { gmb_clipper_set_fixed(gmb_clipper_global_state(), on ? 1 : 0); } catch { } }
    public void ClipperSetTypeahead(int size) { try { gmb_clipper_set_typeahead(gmb_clipper_global_state(), size); } catch { } }
    public void ClipperSetDelimiters(string v) { try { gmb_clipper_set_delimiters(gmb_clipper_global_state(), v ?? ""); } catch { } }
    public void ClipperSetDateFormat(string fmt) { try { gmb_clipper_set_date_format(gmb_clipper_global_state(), fmt ?? ""); } catch { } }
    public void ClipperSetMessage(int row) { try { gmb_clipper_set_message(gmb_clipper_global_state(), row); } catch { } }
    // Nessun ripiego: se la libreria grafica C non si carica l'errore (DllNotFoundException) deve emergere, non
    // diventare un numero vuoto in PRINT. Le dipendenze esterne (libMoltenVK) stanno in Resources/libs/<rid>.
    public string ClipperFormatNumber(double d) { var sb = new System.Text.StringBuilder(128); gmb_clipper_set_format_number(gmb_clipper_global_state(), d, sb, 128); return sb.ToString(); }
    // ---- ClipperMouse (stato C condiviso) ----
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_is_mouse(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrow(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mcol(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleft(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mright(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddle(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleftpress(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrightpress(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddlepress(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleftless(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrightless(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddleless(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleftshift(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrightshift(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddleshift(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleftcontrol(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrightcontrol(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddlecontrol(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mleftm(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrightm(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mmiddlem(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousex(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousey(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousebutton(IntPtr st, int hasId, int buttonId);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousewheel(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mouseclick(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mouseclick_area(IntPtr st, int x1, int y1, int x2, int y2);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousedblclick(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousemove(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousepresent(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mouseleft(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mouseright(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mousemiddle(IntPtr st);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_moustat(IntPtr st, int button, out int row, out int col, out int mods);
    [DllImport(Lib)] private static extern int gmb_clipper_mouse_mrelease(IntPtr st, System.Text.StringBuilder outBuf, int cap);
    [DllImport(Lib)] private static extern void gmb_clipper_mouse_update_simple(IntPtr st, int kind, int x, int y);
    private static IntPtr CS => gmb_clipper_global_state();
    public int ClipperMouseMRow() { try { return gmb_clipper_mouse_mrow(CS); } catch { return 0; } }
    public int ClipperMouseMCol() { try { return gmb_clipper_mouse_mcol(CS); } catch { return 0; } }
    public int ClipperMouseIsMouse() { try { return gmb_clipper_mouse_is_mouse(CS); } catch { return 0; } }
    public int ClipperMouseMLeft() { try { return gmb_clipper_mouse_mleft(CS); } catch { return 0; } }
    public int ClipperMouseMRight() { try { return gmb_clipper_mouse_mright(CS); } catch { return 0; } }
    public int ClipperMouseMMiddle() { try { return gmb_clipper_mouse_mmiddle(CS); } catch { return 0; } }
    public int ClipperMouseMLeftPress() { try { return gmb_clipper_mouse_mleftpress(CS); } catch { return 0; } }
    public int ClipperMouseMRightPress() { try { return gmb_clipper_mouse_mrightpress(CS); } catch { return 0; } }
    public int ClipperMouseMMiddlePress() { try { return gmb_clipper_mouse_mmiddlepress(CS); } catch { return 0; } }
    public int ClipperMouseMLeftLess() { try { return gmb_clipper_mouse_mleftless(CS); } catch { return 0; } }
    public int ClipperMouseMRightLess() { try { return gmb_clipper_mouse_mrightless(CS); } catch { return 0; } }
    public int ClipperMouseMMiddleLess() { try { return gmb_clipper_mouse_mmiddleless(CS); } catch { return 0; } }
    public int ClipperMouseMLeftShift() { try { return gmb_clipper_mouse_mleftshift(CS); } catch { return 0; } }
    public int ClipperMouseMRightShift() { try { return gmb_clipper_mouse_mrightshift(CS); } catch { return 0; } }
    public int ClipperMouseMMiddleShift() { try { return gmb_clipper_mouse_mmiddleshift(CS); } catch { return 0; } }
    public int ClipperMouseMLeftControl() { try { return gmb_clipper_mouse_mleftcontrol(CS); } catch { return 0; } }
    public int ClipperMouseMRightControl() { try { return gmb_clipper_mouse_mrightcontrol(CS); } catch { return 0; } }
    public int ClipperMouseMMiddleControl() { try { return gmb_clipper_mouse_mmiddlecontrol(CS); } catch { return 0; } }
    public int ClipperMouseMLeftM() { try { return gmb_clipper_mouse_mleftm(CS); } catch { return 0; } }
    public int ClipperMouseMRightM() { try { return gmb_clipper_mouse_mrightm(CS); } catch { return 0; } }
    public int ClipperMouseMMiddleM() { try { return gmb_clipper_mouse_mmiddlem(CS); } catch { return 0; } }
    public int ClipperMouseX() { try { return gmb_clipper_mouse_mousex(CS); } catch { return 0; } }
    public int ClipperMouseY() { try { return gmb_clipper_mouse_mousey(CS); } catch { return 0; } }
    public int ClipperMouseButton(int hasId, int buttonId) { try { return gmb_clipper_mouse_mousebutton(CS, hasId, buttonId); } catch { return 0; } }
    public int ClipperMouseWheel() { try { return gmb_clipper_mouse_mousewheel(CS); } catch { return 0; } }
    public int ClipperMouseClick() { try { return gmb_clipper_mouse_mouseclick(CS); } catch { return 0; } }
    public int ClipperMouseClickArea(int x1, int y1, int x2, int y2) { try { return gmb_clipper_mouse_mouseclick_area(CS, x1, y1, x2, y2); } catch { return 0; } }
    public int ClipperMouseDblClick() { try { return gmb_clipper_mouse_mousedblclick(CS); } catch { return 0; } }
    public int ClipperMouseMove() { try { return gmb_clipper_mouse_mousemove(CS); } catch { return 0; } }
    public int ClipperMousePresent() { try { return gmb_clipper_mouse_mousepresent(CS); } catch { return 0; } }
    public int ClipperMouseLeft() { try { return gmb_clipper_mouse_mouseleft(CS); } catch { return 0; } }
    public int ClipperMouseRight() { try { return gmb_clipper_mouse_mouseright(CS); } catch { return 0; } }
    public int ClipperMouseMiddle() { try { return gmb_clipper_mouse_mousemiddle(CS); } catch { return 0; } }
    public int ClipperMouseMouStat(int button, out int row, out int col, out int mods) { row = 0; col = 0; mods = 0; try { return gmb_clipper_mouse_moustat(CS, button, out row, out col, out mods); } catch { return 0; } }
    public string ClipperMouseRelease() { var sb = new System.Text.StringBuilder(16); try { gmb_clipper_mouse_mrelease(CS, sb, 16); } catch { } return sb.ToString(); }
    public void ClipperMouseUpdateEvent(int kind, int x, int y) { try { gmb_clipper_mouse_update_simple(CS, kind, x, y); } catch { } }

    public void FormReset() { FormResetC(); try { gmb_tui_controls_reset(); } catch { } }
    public void FormFrame(int frameMs) => FormFrameC(frameMs);
    public bool FormClosed() => FormClosedC() != 0;
    public void FormClose() => FormRequestCloseC();
    public void FormSetLastStyle(string spec) => FormSetLastStyleC(spec);
    public void FormSetLastStyleForHandle(int handle, string spec) => FormSetLastStyleForHandleC(handle, spec ?? "");
    public double FormControlGet(string name, string prop) => FormControlGetC(name, prop);
    public string FormControlGetStr(string name, string prop) => FormControlGetStrC(name, prop);
    public void FormControlSet(string name, string prop, double num, string str) => FormControlSetC(name, prop, num, str);
    public void FormControlMove(string name, int x, int y, int w, int h) => FormControlMoveC(name, x, y, w, h);
    public void FormControlFocus(string name) => FormControlFocusC(name);
    public void FormControlZorder(string name, int front) => FormControlZorderC(name, front);
    public void FormControlInvalidate(string name) => FormControlInvalidateC(name);
    public void FormControlRefresh(string name) => FormControlRefreshC(name);

    public void FormResetC() => gmb_form_reset();
    public void FormFrameC(int frameMs) => gmb_form_frame(frameMs);
    public void UiFrame(int frameMs) { try { gmb_ui_frame(frameMs); } catch { } }
    /// <summary>WINDOW RESIZE w, h — ridimensiona la pagina LOGICA e riapplica il LAYOUT
    /// (EXPAND/ANCHOR/ALIGN) su entrambi i percorsi. Unica implementazione nel runtime C,
    /// la stessa usata da C99Native e VM/C.</summary>
    public void WindowResize(int w, int h) { try { gmb_window_resize(w, h); } catch { } }

    /// <summary>Contesto grafico passato al metodo Paint di un UserControl (coordinate locali).
    /// Il disegno è accodato al main thread (Enq) → il thread SDL non esegue BASIC (ADR §7).</summary>
    public sealed class UserPaintContext
    {
        public int Width, Height; private int _ox, _oy; private uint _color = 0xFFC8D0DC;
        private readonly Action<Action> _post;
        public UserPaintContext(int x, int y, int w, int h, Action<Action> post)
        { _ox = x; _oy = y; Width = w; Height = h; _post = post; }
        public void SetColor(double c) => _color = (uint)c;
        public void Clear() => _post(() => { gmb_gfx_fill_rounded_rect(_ox, _oy, Width, Height, 0, 0xFF11141D); });
        public void Line(double x1, double y1, double x2, double y2) => _post(() => gmb_gfx_line(_ox + (int)x1, _oy + (int)y1, _ox + (int)x2, _oy + (int)y2, (int)_color, ""));
        public void Rect(double x, double y, double w, double h) => _post(() => gmb_gfx_stroke_rounded_rect(_ox + (int)x, _oy + (int)y, (int)w, (int)h, 0, _color));
        public void FillRect(double x, double y, double w, double h) => _post(() => gmb_gfx_fill_rounded_rect(_ox + (int)x, _oy + (int)y, (int)w, (int)h, 0, _color));
        public void Circle(double cx, double cy, double r) => _post(() => gmb_gfx_circle(_ox + (int)cx, _oy + (int)cy, (int)r, (int)_color, 0, 0.0, 360.0, 1.0));
        public void FillCircle(double cx, double cy, double r) => _post(() => gmb_gfx_fill_circle(_ox + (int)cx, _oy + (int)cy, (int)(r * 2), _color));
        public void Text(string s, double x, double y) => _post(() => gmb_gfx_draw_text(s ?? "", _ox + (int)x, _oy + (int)y, 0, (int)_color));
        public void Plot(double x, double y) => _post(() => gmb_gfx_line(_ox + (int)x, _oy + (int)y, _ox + (int)x, _oy + (int)y, (int)_color, ""));
    }
    public void UiAddCheckbox(int x, int y, int w, string label) { try { gmb_tui_add_checkbox(x, y, w, label ?? ""); } catch { } }
    public double UiLastHandle() { try { return gmb_tui_last_handle(); } catch { return -1; } }
    public void UiControlApplyFocus(int handle) { try { gmb_control_apply_props_handle(handle, 128u, "", "", "", 0, 0, 0, 0); } catch { } }
    public void UiControlApplyStyle(int handle, string? spec) { try { if (spec != null) gmb_form_apply_style_handle(handle, spec); } catch { } }
    public void UiControlApplyProps(int handle, string? name, string? tag, string? tooltip, double? tabindex, double? visible, double? enabled, double? tabstop)
    {
        uint flags = 0; if (name != null) flags |= 1; if (tag != null) flags |= 2; if (tooltip != null) flags |= 4;
        if (tabindex != null) flags |= 8; if (visible != null) flags |= 16; if (enabled != null) flags |= 32; if (tabstop != null) flags |= 64;
        try { gmb_control_apply_props_handle(handle, flags, name ?? "", tag ?? "", tooltip ?? "", (int)(tabindex ?? 0), (visible ?? 0) != 0 ? 1 : 0, (enabled ?? 0) != 0 ? 1 : 0, (tabstop ?? 0) != 0 ? 1 : 0); } catch { }
    }
    public void UiAddTbrowse(int row, int col, int w, int h, string colsCsv, string dataCsv) { try { gmb_tui_add_tbrowse(row, col, w, h, colsCsv ?? "", dataCsv ?? ""); } catch { } }
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern IntPtr gmb_tui_add_textinput(int x, int y, int w, string initial);
    public void UiAddTextInput(int x, int y, int w, string initial) { try { gmb_tui_add_textinput(x, y, w, initial ?? ""); } catch { } }
    [DllImport(Lib)] private static extern int gmb_tui_control_selrow(int i);
    public int UiTbrowseSelRow(int i) { try { return gmb_tui_control_selrow(i); } catch { return -1; } }
    [DllImport(Lib)] private static extern int gmb_tui_control_value(int i);
    /// <summary>Valore/selezione di un controllo registrato (TAB: indice tab attivo). -1 se n/d.</summary>
    public int UiControlValue(int i) { try { return gmb_tui_control_value(i); } catch { return -1; } }
    [DllImport(Lib)] private static extern void gmb_tui_control_error(int i, int line, int on);
    /// <summary>Marca/toglie un errore di lint (squiggle) su una riga del CodeArea registrato.</summary>
    public void UiCodeAreaError(int i, int line, bool on) { try { gmb_tui_control_error(i, line, on ? 1 : 0); } catch { } }
    [DllImport(Lib, CharSet = CharSet.Ansi)] private static extern int gmb_tui_control_text(int i, byte[] outBuf, int cap);
    /// <summary>Testo di un controllo CodeArea/TextField registrato (indice nel registry C).</summary>
    public string UiControlText(int i)
    {
        try
        {
            var b = new byte[1 << 20];
            gmb_tui_control_text(i, b, b.Length);
            int z = Array.IndexOf(b, (byte)0); if (z < 0) z = b.Length;
            return System.Text.Encoding.UTF8.GetString(b, 0, z);
        }
        catch { return ""; }
    }
    /// <summary>True se l'host ha richiesto la chiusura (ESC nella frame cell C / SDL quit).</summary>
    public bool UiClosed() { try { return gmb_form_closed() != 0; } catch { return false; } }
    /// <summary>Azzera il flag di chiusura (gmb_form_reset) — per chiudere un popup su ESC senza uscire.</summary>
    public void UiResetClose() { try { gmb_form_reset(); } catch { } }
    [DllImport(Lib)] private static extern int gmb_tui_host_key_pop();
    /// <summary>Legge un tasto NON consumato dal controllo C focalizzato (shortcut host). 0 = nessuno.</summary>
    public int UiPollKey() { try { return gmb_tui_host_key_pop(); } catch { return 0; } }
    /// <summary>Azzera il registry dei controlli TUI (per ri-registrare la UI).</summary>
    public void UiControlsReset() { try { gmb_tui_controls_reset(); } catch { } }
    [DllImport(Lib)] private static extern int gmb_form_poll_event(out int ev, out int widx);
    /// <summary>Legge un evento UI dal runtime C (gmb_form_poll_event). True se ce n'è uno.</summary>
    public bool UiPollEvent(out int ev, out int widx) { try { return gmb_form_poll_event(out ev, out widx) != 0; } catch { ev = 0; widx = 0; return false; } }
    public void UiAddFiledialog(string title, string mask) { try { gmb_tui_add_filedialog(title ?? "", mask ?? ""); } catch { } }
    public void UiAddPrinterdialog(string title) { try { gmb_tui_add_printerdialog(title ?? ""); } catch { } }
    public void UiAddTreeview(int x, int y, int w, int h, string csv) { try { gmb_tui_add_treeview(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddGrid(int x,int y,int w,int h,int cols,string csv) { try { gmb_tui_add_grid(x, y, w, h, cols, csv ?? ""); } catch { } }
    public void UiAddDatepicker(int x,int y,int w) { try { gmb_tui_add_datepicker(x, y, w); } catch { } }
    public void UiAddColorpicker(int x,int y,int w,int h) { try { gmb_tui_add_colorpicker(x, y, w, h); } catch { } }
    public void UiAddListbox(int x,int y,int w,int h,string csv) { try { gmb_tui_add_listbox(x, y, w, h, csv ?? ""); } catch { } }
    /* Handle del LISTBOX appena creato: stessa sorgente di CONTROL GET "nome","handle"
     * (il runtime C condiviso), così `LISTBOX … INTO v` lega su IL/PCode lo stesso handle
     * di C99Native/VM-C invece del default -1 dell'interfaccia. */
    public int UiAddListboxH(int x,int y,int w,int h,string csv) { try { UiAddListbox(x, y, w, h, csv); return gmb_form_last_handle(); } catch { return -1; } }
    public void UiAddCombobox(int x,int y,int w,string csv) { try { gmb_tui_add_combobox(x, y, w, csv ?? ""); } catch { } }
    public void UiAddRadiogroup(int x,int y,int w,int h,string csv) { try { gmb_tui_add_radiogroup(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddToggle(int x,int y,int w,string caption) { try { gmb_tui_add_toggle(x, y, w, caption ?? ""); } catch { } }
    public void UiAddSlider(int x,int y,int w,int mn,int mx,int val) { try { gmb_tui_add_slider(x, y, w, mn, mx, val); } catch { } }
    public void UiAddNumericUpDown(int x,int y,int w,int mn,int mx,int val) { try { gmb_tui_add_numericupdown(x, y, w, mn, mx, val); } catch { } }
    public void UiAddStarRating(int x,int y,int w,int mx,int val) { try { gmb_tui_add_starrating(x, y, w, mx, val); } catch { } }
    public void UiAddScrollBar(int x,int y,int len,int mn,int mx,int val) { try { gmb_tui_add_scrollbar(x, y, len, mn, mx, val); } catch { } }
    public void UiAddScrollView(int x,int y,int w,int h,string csv) { try { gmb_tui_add_scrollview(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddLabel(int x,int y,int w,string text) { try { gmb_tui_add_label(x, y, w, text ?? ""); } catch { } }
    public void UiAddDataList(int x,int y,int w,int h,string csv) { try { gmb_tui_add_datalist(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddPropertyGrid(int x,int y,int w,int h,string csv) { try { gmb_tui_add_propertygrid(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddSplitter(int x,int y,int w,int h,int orient) { try { gmb_tui_add_splitter(x, y, w, h, orient); } catch { } }
    public void UiAddContextMenu(int x,int y,int w,int h,string csv) { try { gmb_tui_add_contextmenu(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddLogView(int x,int y,int w,int h,string csv) { try { gmb_tui_add_logview(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddHexView(int x,int y,int w,int h,string data) { try { gmb_tui_add_hexview(x, y, w, h, data ?? ""); } catch { } }
    public void UiAddBigText(int x,int y,string text) { try { gmb_tui_add_bigtext(x, y, text ?? ""); } catch { } }
    public void UiAddCodeArea(int x,int y,int w,int h,string csv) { try { gmb_tui_add_codearea(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddReorderListBox(int x,int y,int w,int h,string csv) { try { gmb_tui_add_reorderlistbox(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddAlertBox(int x,int y,int w,int h,string text) { try { gmb_tui_add_alertbox(x, y, w, h, text ?? ""); } catch { } }
    public void UiAddConfirmationBox(int x,int y,int w,int h,string text) { try { gmb_tui_add_confirmationbox(x, y, w, h, text ?? ""); } catch { } }
    public void UiAddFileExplorer(int x,int y,int w,int h,string path) { try { gmb_tui_add_fileexplorer(x, y, w, h, path ?? ""); } catch { } }
    public void UiAddDashboard(int x,int y,int w,int h,string csv) { try { gmb_tui_add_dashboard(x, y, w, h, csv ?? ""); } catch { } }
    public void UiAddSpinner(int x,int y,int w,string preset) { try { gmb_tui_add_spinner(x, y, w, preset ?? ""); } catch { } }
    public void UiAddStatusbar(int x,int y,int w,string text) { try { gmb_tui_add_statusbar(x, y, w, text ?? ""); } catch { } }
    public void UiAddTab(int x,int y,int w,string csv) { try { gmb_tui_add_tab(x, y, w, csv ?? ""); } catch { } }
    public void UiAddMenu(int x,int y,int w,string csv) { try { gmb_tui_add_menu(x, y, w, csv ?? ""); } catch { } }
    public int  FormClosedC() { try { return gmb_form_closed(); } catch { return 1; } }
    public void FormRequestCloseC() => gmb_form_request_close();
    public void FormSetLastStyleC(string spec) => gmb_form_set_last_style(spec);
    public void FormSetLastStyleForHandleC(int handle, string spec) => gmb_form_apply_style_handle(handle, spec);
    public double FormControlGetC(string name, string prop) { try { return gmb_form_control_get(name, prop); } catch { return 0; } }
    public void FormControlSetC(string name, string prop, double num, string str) => gmb_form_control_set(name, prop, num, str ?? "");
    public string FormControlGetStrC(string name, string prop) { try { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gmb_form_control_get_str(name, prop)) ?? ""; } catch { return ""; } }
    public void FormControlMoveC(string name, int x, int y, int w, int h) => gmb_form_control_move(name, x, y, w, h);
    public void FormControlFocusC(string name) => gmb_form_control_focus(name);
    public void FormControlZorderC(string name, int front) => gmb_form_control_zorder(name, front);
    public void FormControlInvalidateC(string name) => gmb_form_control_invalidate(name);
    public void FormControlRefreshC(string name) => gmb_form_control_refresh(name);
    public double FormLastHandle() { try { return gmb_form_last_handle(); } catch { return -1; } }
    // ── Bridge eventi nativo→managed: thunk C che rientra nel managed ──
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate double GfxEventThunkDelegate();
    private static GfxEventThunkDelegate? _evThunkKeepAlive;
    private static IntPtr _evThunkPtr = IntPtr.Zero;
    /// <summary>Sink invocato quando un evento controllo/bottone scatta (control, eventIndex).</summary>
    public static Action<int, int>? OnControlEvent;
    private static double GfxEventThunk()
    { try { OnControlEvent?.Invoke(gmb_form_firing_control(), gmb_form_firing_event()); } catch { } return 0; }
    private static IntPtr EventThunkPtr()
    {
        if (_evThunkPtr == IntPtr.Zero)
        { _evThunkKeepAlive = GfxEventThunk; _evThunkPtr = Marshal.GetFunctionPointerForDelegate(_evThunkKeepAlive); }
        return _evThunkPtr;
    }
    public void FormRegisterEventThunk(int handle, int ev) { try { gmb_form_set_event_id_ptr(handle, ev, EventThunkPtr()); } catch { } }
    public void FormSetControlEventSink(Action<int, int> sink) { OnControlEvent = sink; }
    public void FormUnregisterEventThunk(int handle, int ev) { try { gmb_form_set_event_id_ptr(handle, ev, IntPtr.Zero); } catch { } }
    public void FormFireEvent(int handle, int ev) { try { gmb_form_fire_event(handle, ev); } catch { } }
    public void FormSetLastEvent(int handle, int ev) { try { gmb_form_set_last_event(handle, ev); } catch { } }
    public int FormEventIndex(string name) { try { return gmb_form_ev_index(name); } catch { return 0; } }
    public int FormFiringControl() { try { return gmb_form_firing_control(); } catch { return -1; } }
    public int FormFiringEvent() { try { return gmb_form_firing_event(); } catch { return 0; } }
    public double FormClicked(int h) { try { return gmb_form_clicked(h); } catch { return 0; } }
    public double FormDblClicked(int h) { try { return gmb_form_dblclicked(h); } catch { return 0; } }
    public double FormChanged(int h) { try { return gmb_form_changed(h); } catch { return 0; } }
    public double FormGotFocused(int h) { try { return gmb_form_gotfocused(h); } catch { return 0; } }
    public double FormLostFocused(int h) { try { return gmb_form_lostfocused(h); } catch { return 0; } }
    public double MouseClicked() { try { return gmb_mouse_clicked(); } catch { return 0; } }
    public double MouseDblClicked() { try { return gmb_mouse_dblclicked(); } catch { return 0; } }
    public double MousePressed() { try { return gmb_mouse_pressed(); } catch { return 0; } }
    public double KeyPressed() { try { return gmb_key_pressed(); } catch { return 0; } }

    [DllImport(Lib)] private static extern void gmb_form_add_checkbox(int x,int y,string label,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_radio(int x,int y,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_track(int x,int y,int w,double vmin,double vmax,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_dropdown(int x,int y,int w,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_progress(int x,int y,int w,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_progress_v(int x,int y,int w,double value);
    [DllImport(Lib)] private static extern void gmb_form_add_toggle(int x,int y,string label,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_spinner(int x,int y,int w,double vmin,double vmax,double step,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_statusbar(int x,int y,int w,string text);
    [DllImport(Lib)] private static extern void gmb_form_add_tabs(int x,int y,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_listbox(int x,int y,int w,int h,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_colorpicker(int x,int y,string palette,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_datepicker(int x,int y,IntPtr strptr,string fmt);
    [DllImport(Lib)] private static extern void gmb_form_add_grid(int x,int y,int w,int h,string headers,string data,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_tree(int x,int y,int w,int h,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_menubar(int x,int y,string items,IntPtr var);
    [DllImport(Lib)] private static extern void gmb_form_add_filedialog(int x,int y,int w,int h,string dir,IntPtr strptr);
    [DllImport(Lib)] private static extern void gmb_form_add_textbox(int x,int y,int w,int h,IntPtr strptr);
    [DllImport(Lib)] private static extern void gmb_form_add_button(int x,int y,int w,int h,string label,IntPtr cb);
    [DllImport(Lib)] private static extern void gmb_tui_add_canvas(int x,int y,int w,int h);
    [DllImport(Lib)] private static extern void gmb_tui_add_image(int x,int y,int w,int h,string label);
    [DllImport(Lib)] private static extern void gmb_tui_add_imagebutton(int x,int y,int w,int h,string label);
    [DllImport(Lib)] private static extern void gmb_tui_add_qr(int x,int y,int w,int h,string text);
    [DllImport(Lib)] private static extern void gmb_tui_add_richtext(int x,int y,int w,int h,string text);
    [DllImport(Lib)] private static extern void gmb_tui_add_halfblock(int x,int y,int w,int h,string label);
    [DllImport(Lib)] private static extern void gmb_tui_add_tiling(int x,int y,int w,int h,string label);
    [DllImport(Lib)] private static extern void gmb_form_add_panel(int x,int y,int w,int h,string title);
    [DllImport(Lib)] private static extern void gmb_tui_add_usercontrol(int x,int y,int w,int h, IntPtr paint, IntPtr instance, string container, string name);
    public int UserControlLastHandle() { try { return gmb_tui_last_handle(); } catch { return -1; } }

    /// <summary>Crea un controllo pixel per NOME (gambe gestite): argomenti generici; il
    /// valore e' letto per nome (CONTROL GET) dopo FormFrame — nessun puntatore INTO.</summary>
    public void FormDispatchMouse(int type, int x, int y, int button, int wheel) { try { gmb_form_dispatch_mouse_px(type, x, y, button, wheel); } catch { } }

    public void FormDispatchKey(string key) { try { gmb_form_dispatch_key(key ?? ""); } catch { } }

    public void FormAddControl(string kind, object[] args, string name)
    {
        int I(int i) => i < args.Length && args[i] != null ? (int)Convert.ToDouble(args[i]) : 0;
        double D(int i) => i < args.Length && args[i] != null ? Convert.ToDouble(args[i]) : 0;
        string S(int i) => i < args.Length && args[i] != null ? Convert.ToString(args[i]) ?? "" : "";
        {
            switch ((kind ?? "").ToUpperInvariant())
            {
                case "CHECK": gmb_form_add_checkbox(I(0), I(1), S(2), IntPtr.Zero); break;
                case "RADIO": case "RADIOGROUP": gmb_form_add_radio(I(0), I(1), S(2), IntPtr.Zero); break;
                case "TRACK": case "SLIDER": gmb_form_add_track(I(0), I(1), I(2), D(3), D(4), IntPtr.Zero); break;
                case "DROPDOWN": case "COMBOBOX": gmb_form_add_dropdown(I(0), I(1), I(2), S(3), IntPtr.Zero); break;
                case "PROGRESS": gmb_form_add_progress_v(I(0), I(1), I(2), D(3)); break;
                case "TOGGLE": gmb_form_add_toggle(I(0), I(1), S(2), IntPtr.Zero); break;
                case "SPINNER": gmb_form_add_spinner(I(0), I(1), I(2), D(3), D(4), D(5), IntPtr.Zero); break;
                case "TAB": gmb_form_add_tabs(I(0), I(1), S(2), IntPtr.Zero); break;
                case "LISTBOX": gmb_form_add_listbox(I(0), I(1), I(2), I(3), S(4), IntPtr.Zero); break;
                case "COLORPICKER": gmb_form_add_colorpicker(I(1), I(0), null, IntPtr.Zero); break;
                case "GRID": case "TBROWSE": gmb_form_add_grid(I(0), I(1), I(2), I(3), S(4), S(5), IntPtr.Zero); break;
                case "TREEVIEW": gmb_form_add_tree(I(0), I(1), I(2), I(3), S(4), IntPtr.Zero); break;
                case "STATUSBAR": gmb_form_add_statusbar(I(0), I(1), I(2), S(3)); break;
                case "FUNCTIONKEYBAR": gmb_tui_add_fkeybar(I(0), I(1), I(2), S(3)); break;
                case "STARRATING": gmb_tui_add_starrating(I(0), I(1), I(2), I(3), I(4)); break;
                case "CANDLESTICK": gmb_tui_add_candlestick(I(0), I(1), I(2), I(3), S(4)); break;
                case "CHART": gmb_tui_add_chart(I(0), I(1), I(2), I(3), S(4)); break;
                case "MENU": gmb_form_add_menubar(I(0), I(1), S(2), IntPtr.Zero); break;
                case "DATEPICKER": gmb_form_add_datepicker(I(0), I(1), IntPtr.Zero, args.Length > 2 ? S(2) : "dd/mm/yyyy"); break;
                case "FILEDIALOG": gmb_form_add_filedialog(I(0), I(1), I(2), I(3), S(4), IntPtr.Zero); break;
                case "TEXTBOX": gmb_form_add_textbox(I(0), I(1), I(2), I(3), IntPtr.Zero); break;
                case "BUTTON": gmb_form_add_button(I(0), I(1), I(2), I(3), S(4), IntPtr.Zero); break;
                case "CANVAS": gmb_tui_add_canvas(I(0), I(1), I(2), I(3)); break;
                case "IMAGE": gmb_tui_add_image(I(0), I(1), I(2), I(3), S(4)); break;
                case "IMAGEBUTTON": gmb_tui_add_imagebutton(I(0), I(1), I(2), I(3), S(4)); break;
                case "QR": gmb_tui_add_qr(I(0), I(1), I(2), I(3), S(4)); break;
                case "RICHTEXT": gmb_tui_add_richtext(I(0), I(1), I(2), I(3), S(4)); break;
                case "HALFBLOCK": gmb_tui_add_halfblock(I(0), I(1), I(2), I(3), S(4)); break;
                case "TILING": gmb_tui_add_tiling(I(0), I(1), I(2), I(3), S(4)); break;
                case "PANEL": gmb_form_add_panel(I(0), I(1), I(2), I(3), S(4)); break;
            }
            if (!string.IsNullOrEmpty(name)) gmb_form_set_last_style("name=" + name);
        }
    }

    /// <summary>CONTROL ADD: registra un UserControl (istanza di classe BASIC con metodo Paint).
    /// Il metodo Paint viene invocato a ogni frame con un GraphicsContext (coordinate locali).</summary>
    private readonly System.Collections.Generic.List<(object inst, int x, int y, int w, int h, string container, string name)> _userControls = new();
    public void UserControlAdd(object? instance, int x, int y, int w, int h, string container = "", string name = "")
    {
        if (instance == null) return;
        try
        {
            gmb_tui_add_usercontrol(x, y, w, h, IntPtr.Zero, IntPtr.Zero, container ?? "", name ?? "");
            _userControls.Add((instance, x, y, w, h, container ?? "", name ?? ""));
        }
        catch { }
    }
    public void UserControlRemove(object? instance)
    {
        if (instance == null) return;
        try { _userControls.RemoveAll(t => ReferenceEquals(t.inst, instance)); } catch { }
    }

    // ── GraphicsContext per handle (CONTROL DRAW) ──
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_clear(int h);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_setcolor(int h, double c);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_line(int h, double x1, double y1, double x2, double y2);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_rect(int h, double x, double y, double w, double hh);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_fillrect(int h, double x, double y, double w, double hh);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_circle(int h, double cx, double cy, double r);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_fillcircle(int h, double cx, double cy, double r);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_text(int h, string s, double x, double y);
    [DllImport(Lib)] private static extern void gmb_gfxctx_h_plot(int h, double x, double y);
    public void UserControlDraw(int h, int op, double a, double b, double c, double d, string s)
    {
        try
        {
            switch (op)
            {
                case 0: gmb_gfxctx_h_clear(h); break;
                case 1: gmb_gfxctx_h_setcolor(h, a); break;
                case 2: gmb_gfxctx_h_line(h, a, b, c, d); break;
                case 3: gmb_gfxctx_h_rect(h, a, b, c, d); break;
                case 4: gmb_gfxctx_h_fillrect(h, a, b, c, d); break;
                case 5: gmb_gfxctx_h_circle(h, a, b, c); break;
                case 6: gmb_gfxctx_h_fillcircle(h, a, b, c); break;
                case 7: gmb_gfxctx_h_text(h, s ?? "", a, b); break;
                case 8: gmb_gfxctx_h_plot(h, a, b); break;
            }
        }
        catch { }
    }

    /// <summary>Invoca il metodo Paint di ogni UserControl registrato (main thread, prima del frame).</summary>
    private void PaintUserControls()
    {
        if (_userControls.Count == 0) return;
        foreach (var uc in _userControls.ToArray())
        {
            try
            {
                var m = uc.inst.GetType().GetMethod("Paint",
                    System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.IgnoreCase);
                if (m == null) continue;
                var ps = m.GetParameters();
                if (ps.Length == 0) { m.Invoke(uc.inst, null); continue; }
                var (ox, oy) = UserControlAbsOrigin(uc);
                m.Invoke(uc.inst, new object?[] { new UserPaintContext(ox, oy, uc.w, uc.h, Enq) });
            }
            catch { }
        }
    }

    /// <summary>Origine assoluta di un UserControl: somma ricorsivamente l'origine del contenitore
    /// (un altro UserControl per nome, via PARENT).</summary>
    private (int x, int y) UserControlAbsOrigin((object inst, int x, int y, int w, int h, string container, string name) uc)
    {
        int x = uc.x, y = uc.y;
        if (!string.IsNullOrEmpty(uc.container))
        {
            var p = _userControls.Find(t => t.name == uc.container);
            if (p.inst != null) { var (px, py) = UserControlAbsOrigin(p); x += px; y += py; }
        }
        return (x, y);
    }

    // ── Main thread (SDL2) ──
    [DllImport(Lib)] private static extern void gmb_gfx_present_start();
    [DllImport(Lib)] private static extern void gmb_gfx_present_tick();
    [DllImport(Lib)] private static extern int gmb_gfx_present_should_quit();
    [DllImport(Lib)] private static extern void gmb_gfx_present_stop();
    [DllImport(Lib)] private static extern int gmb_gfx_screen_on();
    [DllImport(Lib)] private static extern int gmb_gfx_current_mode();

    /// <summary>True se lo schermo grafico C e' attivo (usato da IlRuntime per
    /// MOUSE.X()/Y()/PRESENT() in modalita' motore C).</summary>
    public static bool IsScreenOnC { get { try { return gmb_gfx_screen_on() != 0; } catch { return false; } } }

    // ── Helper per il router PCodeVM ──
    public void ScreenSetPages(int? visible, int? active) => Enq(() => gmb_gfx_screenset(visible ?? 0, active ?? 0));
    public void CircleEx(int x, int y, int r, int color, double start, double end, double aspect)
        => Enq(() => gmb_gfx_circle(x, y, r, color, 0, start, end, aspect));
    public void UnloadTextureC(int textureId) => Enq(() => gmb_gfx_unload_texture(textureId));
    public void UnloadFontC() => Enq(() => gmb_gfx_font(null!, 0));
    public double CharCellWidthC { get { var w = TextWidth("M"); return w > 0 ? w : 8; } }
    public double CharCellHeightC { get { var h = TextHeight(); return h > 0 ? h : 16; } }
    public int ScreenWidthC { get { try { return gmb_gfx_screen_width(); } catch { return 0; } } }
    public int ScreenHeightC { get { try { return gmb_gfx_screen_height(); } catch { return 0; } } }
    public (int X, int Y) CursorPositionC { get { try { return (gmb_gfx_get_cursor_x(), gmb_gfx_get_cursor_y()); } catch { return (0, 0); } } }
    public void ChartFinancialC(long handle, double[] ohlc, string plotType, string legend)
        => Enq(() => gmb_gfx_chart_plot_financial((int)handle, ohlc, ohlc.Length, plotType ?? "", legend ?? ""));

    /// <summary>Modalità SCREEN corrente del motore C (0 se off).</summary>
    public static int CurrentModeC { get { try { return gmb_gfx_current_mode(); } catch { return 0; } } }

    /// <summary>Resolver delle matrici 2x3 (impostato dal runtime IL: gli id arrivano dal
    /// builtin MATRIX, il cui registry vive in IlBackendRuntime). Se null → identita'.</summary>
    public static Func<long, double[]?>? MatrixResolver;

    /// <summary>Drain input del motore C, invocato da MainThreadTick DOPO il present
    /// (SOLO main thread). Il runtime IL lo collega a IlRuntime.DrainCGfxInput via
    /// BindCGfxInput: tasti → QueueInkey, mouse → stato+dispatch, Ctrl+C → tasto/abort.
    /// Cosi' qualunque consumer della pump C (runner, Executor futuro) riceve l'input
    /// senza doverlo ricordare.</summary>
    public static Action? InputDrain;

    private int _tilemapHandle = -1;

    // ── Code main-thread (SDL SOLO qui): _ops eseguite prima del present,
    //    _afterPresent completate DOPO il present (barriera SYNC). Nessun wait. ──
    private static readonly ConcurrentQueue<Action> _ops = new();
    private static readonly object _opsDrainLock = new();
    private static readonly ConcurrentQueue<Action> _afterPresent = new();
    private static int _mainThreadId;
    private static volatile bool _mainActive;

    /// <summary>Motore grafico di default per IL/PCode/VM-C. Da 2026-10 il motore C
    /// (libgmb_gfx) e' il DEFAULT; impostare GMB_GFX_C=0 (o false/off) per usare il
    /// vecchio motore C# (CommonGraphics.SdlGraphicsService). GMB_GFX_C=1 = motore C.</summary>
    public static bool UseCGraphics
    {
        get
        {
            var v = Environment.GetEnvironmentVariable("GMB_GFX_C");
            if (string.IsNullOrEmpty(v)) return true;                  // default: motore C
            return !(v == "0" || v.Equals("false", StringComparison.OrdinalIgnoreCase)
                              || v.Equals("off", StringComparison.OrdinalIgnoreCase));
        }
    }

    static CGfxGraphicsService()
    {
        CGfxNative.EnsureResolver();
    }

    // ── API main-thread (chiamare SOLO dal main thread) ──
    public static void MainThreadStart()
    {
        _mainThreadId = Environment.CurrentManagedThreadId;
        _mainActive = true;
        gmb_gfx_init();
        gmb_gfx_present_start();
    }

    public static void MainThreadTick()
    {
        while (_ops.TryDequeue(out var op))
        {
            try { op(); } catch { /* non far cadere il pump per una draw fallita */ }
        }
        gmb_gfx_present_tick();
        try { InputDrain?.Invoke(); } catch { /* input mai fatale per la pump */ }
        while (_afterPresent.TryDequeue(out var op))
        {
            try { op(); } catch { }
        }
    }

    public static bool MainThreadShouldQuit() => gmb_gfx_present_should_quit() != 0;

    /// <summary>Esegue un'operazione sul main thread e completa DOPO l'esecuzione
    /// (park-based, mai attese): usato dal router PCodeVM per le operazioni senza
    /// variante *Gpm dedicata.</summary>
    public void RunGpm(Action op, Action<object?, Exception?> completion)
        => Enq(() => { try { op(); completion(null, null); } catch (Exception ex) { completion(null, ex); } });

    /// <summary>Come <see cref="RunGpm(Action, Action{object?, Exception?})"/> ma con
    /// valore di ritorno passato alla completion.</summary>
    public void RunGpm(Func<object?> op, Action<object?, Exception?> completion)
        => Enq(() => { try { completion(op(), null); } catch (Exception ex) { completion(null, ex); } });
    public static void MainThreadStop() { _mainActive = false; gmb_gfx_present_stop(); }
    public static int SavePng(string path) => gmb_gfx_save_screen_png(path);

    private static bool OnMain => _mainActive && Environment.CurrentManagedThreadId == _mainThreadId;

    // Esegue sul main thread se possibile, altrimenti accoda (fire-and-forget).
    // MAI attese: in GPM i metodi con risultato usano la variante *Gpm (park).
    private static void Enq(Action op)
    {
        if (!_mainActive || OnMain) op();
        else _ops.Enqueue(op);
    }

    // Completamento DOPO il present del frame (barriera SYNC).
    private static void EnqAfterPresent(Action op)
    {
        if (!_mainActive) op();
        else _afterPresent.Enqueue(op);
    }

    private static int C(object? v)
    {
        switch (v)
        {
            case null: return -1;
            case System.Collections.IDictionary d:
                {
                    // UDT RGBA (Red/Green/Blue/Alpha) → 0x40000000|RGB.
                    double Cv(object? o) => o is null ? 0.0 : Convert.ToDouble(o);
                    var r = (int)Cv(d.Contains("Red") ? d["Red"] : null);
                    var g = (int)Cv(d.Contains("Green") ? d["Green"] : null);
                    var b = (int)Cv(d.Contains("Blue") ? d["Blue"] : null);
                    return RgbFlag | (r << 16) | (g << 8) | b;
                }
            case int i: return i;
            case long l: return (int)l;
            case double d: return (int)d;
            case System.Collections.IEnumerable e:
                {
                    int r = 0, g = 0, b = 0, n = 0;
                    foreach (var o in e)
                    {
                        var x = Convert.ToInt32(o);
                        if (n == 0) r = x; else if (n == 1) g = x; else if (n == 2) b = x;
                        n++;
                    }
                    return n >= 3 ? RgbFlag | (r << 16) | (g << 8) | b : -1;
                }
            default: return -1;
        }
    }

    // ── IGraphicsService (drawing: accodato; getter: round-trip) ──
    public void Screen(int mode, int width, int height, int bpp) => Enq(() => ScreenCore(mode, width, height, bpp));
    public void ScreenCoords(int mode) => Enq(() => gmb_gfx_set_coord_mode(mode));
    public void TextMode(int mode) => Enq(() => gmb_gfx_console_select(mode));

    private static void ScreenCore(int mode, int width, int height, int bpp)
    {
        // Parità SdlGraphicsService: dimensioni esplicite (>8) con mode 0/-1 => modalità grafica
        // (il C usa 0 = text-mode, -1 = desktop).
        var m = mode;
        if ((width > 8 || height > 8) && m <= 0) m = 12;
        gmb_gfx_screen(m, width, height, bpp);
    }
    public void PSet(int x, int y, int color) => Enq(() => gmb_gfx_pset(x, y, color));
    public void Preset(int x, int y) => Enq(() => gmb_gfx_preset(x, y));
    /// <summary>Lettura SINCRONA del pixel dalla pagina software C (CPU-only, nessuna SDL):
    /// usata dai builtin POINT(x,y) dei backend (parità 4 backend sul motore C). Prima
    /// drena gli op grafici accodati (tutti CPU-only: disegno sulla pagina software):
    /// senza, il worker POINT leggeva una pagina non ancora creata/aggiornata (SCREEN/PSET
    /// accodati) → -1. Il drain è serializzato con quello del main thread.</summary>
    public int PointDirect(int x, int y)
    {
        lock (_opsDrainLock)
        {
            while (_ops.TryDequeue(out var op)) { try { op(); } catch { } }
        }
        try { return gmb_gfx_point(x, y); } catch { return -1; }
    }

    /// <summary>CANVAS: operazioni su canvas offscreen. Pure (nessuna finestra/SDL), quindi
    /// chiamabili direttamente da qualunque thread: prima si drenano le ops accodate per
    /// restare coerenti con le primitive già in coda, poi si chiama il runtime C.</summary>
    public static double CanvasOpNative(string op, double a0, double a1, double a2, double a3, string path)
    {
        lock (_opsDrainLock)
        {
            while (_ops.TryDequeue(out var queued)) { try { queued(); } catch { } }
        }
        try { return gmb_gfx_canvas_op(op ?? "", new[] { a0, a1, a2, a3 }, 4, path ?? ""); }
        catch { return -1; }
    }

    public int Point(int x, int y)
        => throw new InvalidOperationException(
            "POINT() richiede il parcheggio GPM (IlGpmRuntime.TryPointPark). Non si attende il thread .NET.");
    public void Line(int x1, int y1, int x2, int y2, int color, string? style) => Enq(() => gmb_gfx_line(x1, y1, x2, y2, color, style));
    // Cerchio intero: il C interpreta start==end==0 come full (unità RADIANTI, come CircleInternal).
    public void Circle(int x, int y, int radius, int color) => Enq(() => gmb_gfx_circle(x, y, radius, color, 0, 0, 0, 1.0));
    public void Paint(int x, int y, int fillColor, int borderColor) => Enq(() => gmb_gfx_paint(x, y, fillColor, borderColor));
    public void Draw(string command) => Enq(() => gmb_gfx_draw(command));
    public void PCopy(int srcPage, int destPage) => Enq(() => gmb_gfx_pcopy(srcPage, destPage));
    public void ScreenSet(int width, int height, int bpp) => Enq(() => gmb_gfx_screenset(width, height));
    public void Font(string name, int size) => Enq(() => gmb_gfx_font(ResolveFontPath(name), size));

    /// <summary>Risolve il path del font come FontInternal del motore C# (i sample usano
    /// "font.ttf" relativo): prova AppContext/Resources, CWD/Resources, "Resources/".</summary>
    private static string ResolveFontPath(string name)
    {
        if (string.IsNullOrEmpty(name)) return name;
        try
        {
            if (System.IO.File.Exists(name)) return name;
            var candidates = new[]
            {
                System.IO.Path.Combine(AppContext.BaseDirectory, name),
                System.IO.Path.Combine(AppContext.BaseDirectory, "Resources", name),
                System.IO.Path.Combine(Directory.GetCurrentDirectory(), "Resources", name),
                System.IO.Path.Combine("Resources", name),
            };
            foreach (var c in candidates)
                if (System.IO.File.Exists(c)) return c;
        }
        catch { }
        return name;
    }
    public void Palette(int index, int r, int g, int b) => Enq(() => gmb_gfx_palette(index, r, g, b));
    public void SetPaletteColor(int index, int r, int g, int b) => Enq(() => gmb_gfx_palette(index, r, g, b));
    public void Clear(int? color) => Enq(() => gmb_gfx_cls(color ?? -1));
    public void DrawText(string text, int x, int y, double align) => Enq(() => gmb_gfx_draw_text(text, x, y, (int)align, -1));
    // Metriche font: lettura CPU del font corrente (nessuna chiamata SDL) → diretta.
    /* TEXTWIDTH/TEXTHEIGHT BASIC = coordinate LOGICHE (parità VM/C/C99: il runtime C applica
     * la scala render). Prima si usava la variante NATIVA → IL/PCode divergevano dalla VM/C. */
    public double TextWidth(string text) => gmb_gfx_text_width_logical(text);
    public double TextHeight() => gmb_gfx_text_height_logical();
    public void Sync() => Enq(() => gmb_gfx_sync());
    /// <summary>SYNC con barriera: ritorna dopo che il motore C ha eseguito il frame (anche se il pump SDL gira su un altro
    /// thread). Serve a chi deve leggere subito l'esito del SYNC (collisioni sprite) fuori dallo stepper GPM.</summary>
    public void SyncBarrier()
    {
        if (!_mainActive || OnMain) { gmb_gfx_sync(); return; }
        using var done = new System.Threading.ManualResetEventSlim(false);
        Enq(() => gmb_gfx_sync());
        EnqAfterPresent(() => done.Set());
        done.Wait(5000);
    }
    public void WindowVisible(bool visible) => Enq(() => gmb_gfx_window_set_visible(visible ? 1 : 0));
    public bool IsAvailable => true;

    /// <summary>Spegne la modalita' grafica NATIVA in modo SINCRONO (nessuna Enq/main-thread
    /// queue): usato dagli harness per isolare le gambe. Con Enq il comando poteva restare in
    /// coda se il main thread era attivo → SCREEN restava ON e TEXTWIDTH/PRINT deviavano.</summary>
    public static void ForceScreenOff() { try { gmb_gfx_screen(0, 0, 0, 0); } catch { } }
    /// <summary>Metrica LOGICA del motore C (TEXTWIDTH/TEXTHEIGHT BASIC), usabile anche senza
    /// un host grafico attivo: sorgente unica per IL/PCode (parità VM/C/C99).</summary>
    public static double TextWidthLogical(string? text) { try { return gmb_gfx_text_width_logical(text ?? ""); } catch { return (text?.Length ?? 0) * 8; } }
    public static double TextHeightLogical() { try { return gmb_gfx_text_height_logical(); } catch { return 16; } }
    public void ViewPort(int x1, int y1, int x2, int y2) => Enq(() => gmb_gfx_view(x1, y1, x2, y2));
    public void Window(int x1, int y1, int x2, int y2) => Enq(() => gmb_gfx_window(x1, y1, x2, y2));
    public void BSave(int x1, int y1, int x2, int y2) => Enq(() => gmb_gfx_bsave(x1, y1, x2, y2));
    public void BLoad(int x, int y) => Enq(() => gmb_gfx_bload(x, y));
    public void SaveScreen(string path) => Enq(() => gmb_gfx_save_screen_png(path));
    public void RestoreScreen(string path) => Enq(() => gmb_gfx_restore_screen_png(path));
    public void CursorState(bool visible) => Enq(() => gmb_gfx_set_cursor_state(visible ? 1 : 0));
    public void CursorShape(int shape) => Enq(() => gmb_gfx_set_cursor_shape(shape));
    public void SetAttribute(int attr) => Enq(() => gmb_gfx_set_attribute(attr));
    public void Locate(int col, int line) => Enq(() => gmb_gfx_locate(col, line));
    // A schermo C OFF (es. dopo SCREEN 0) il testo va su stdout (Console), come in headless
    // (NullGraphicsService): gmb_gfx_print a screen-off non emette nulla → il PRINT spariva
    // (parità IL/PCode/VM-C sull'output dei minitest grafici).
    public void Print(string? text) => Enq(() =>
    {
        /* text == null = newline (PrintNewLineDispatch fa Print(null)); come il motore
         * C# (SdlGraphicsService.Print usa text ?? "\n"). Prima emetteva "" -> nessun
         * avanzamento del cursore e i PRINT si sovrapponevano (divergenza GraphicsColors). */
        var t = text ?? "\n";
        if (IsScreenOnC) gmb_gfx_print(t);
        else Console.Write(t);
    });
    public void Color(object? foreground, object? background) => Enq(() => gmb_gfx_color(C(foreground), C(background)));
    public void GraphicsTab(int col) => Enq(() =>
    {
        var cw = gmb_gfx_text_width("M");
        if (cw <= 0) cw = 8;
        gmb_gfx_locate(col * cw, gmb_gfx_get_cursor_y());
    });
    public void GraphicsTabZone() => Enq(() => gmb_gfx_tab_zone());
    public void CPrint(string text) => Console.Write(text ?? "");   /* console/stdout come SdlGraphicsService (non la pagina) */
    public void CPrintNewLine() => Console.WriteLine();
    public void CLocate(int line, int col) => Enq(() => gmb_gfx_locate(col, line));
    public void CCls() => Enq(() => gmb_gfx_cls(-1));
    public void CSaveScreen(string path) => Enq(() => gmb_gfx_save_screen_png(path));
    public void CRestoreScreen(string path) => Enq(() => gmb_gfx_restore_screen_png(path));
    public void ClearRect(int x1, int y1, int x2, int y2) => Enq(() => gmb_gfx_clear_rect(x1, y1, x2, y2, -1));

    public int[] ImageGet(int x1, int y1, int x2, int y2)
    {
        var w = x2 - x1 + 1; var h = y2 - y1 + 1;
        if (w <= 0 || h <= 0) return Array.Empty<int>();
        var buf = new int[2 + w * h];
        var n = gmb_gfx_image_get(x1, y1, x2, y2, buf, buf.Length);
        return n < 2 ? Array.Empty<int>() : buf;
    }
    public void ImagePut(int x, int y, int[] img, int op) => Enq(() => gmb_gfx_image_put(x, y, img, img.Length));
    public void ImagePuts(int[] spriteData, int[] positions, int op) => Enq(() =>
    {
        var pos = new float[positions.Length];
        for (var i = 0; i < positions.Length; i++) pos[i] = positions[i];
        // count = NUMERO di sprite (positions è flat x,y): passare positions.Length
        // leggeva oltre l'array in C → SIGBUS (crash 2026-10-03 su GraphicsPutsVsTextures).
        gmb_gfx_image_puts_n(spriteData, spriteData.Length, pos, positions.Length / 2);
    });
    public void DrawTexture(int x, int y, int textureId) => Enq(() => gmb_gfx_draw_texture(textureId, x, y, 0, 1, 1));
    public void DrawTextureEx(int textureId, int x, int y, double rotation, double scaleX, double scaleY, int color)
        => Enq(() => gmb_gfx_draw_texture(textureId, x, y, rotation, scaleX, scaleY));
    // Decodifica PNG + registry texture: CPU puro (nessuna SDL) → diretta.
    // Nel percorso GPM il runtime usa LoadTextureGpm (park, completion sul main).
    public int LoadTexture(string path, int textureId) => gmb_gfx_load_texture(path);
    public void DrawTextures(int x, int y, int textureId, int frame) => Enq(() => gmb_gfx_draw_texture_frame(textureId, frame, x, y));
    public void Polygon(float[] points, int color) => Enq(() => gmb_gfx_polygon(points, points.Length / 2, color));
    public void DrawPixelBuffer(int width, int height, object? buffer)
    {
        if (buffer is not System.Collections.IEnumerable e) return;
        var px = new uint[width * height];
        var i = 0;
        foreach (var o in e) { if (i >= px.Length) break; px[i++] = (uint)Convert.ToInt64(o); }
        Enq(() => gmb_gfx_draw_pixel_buffer(width, height, px));
    }

    // Sprite
    public void SpriteDef(int spriteId, int width, int height, int[] pixels)
        => Enq(() => gmb_gfx_sprite_def(spriteId, width, height, pixels));
    public int SpriteLoad(string filePath, int spriteId) => gmb_gfx_sprite_load(filePath);
    public void SpriteSetPosition(int spriteId, float x, float y) => Enq(() => gmb_gfx_sprite_set_position(spriteId, x, y));
    public void SpriteSetVisible(int spriteId, bool visible) => Enq(() => gmb_gfx_sprite_set_visible(spriteId, visible ? 1 : 0));
    public void SpriteSetPriority(int spriteId, int priority) => Enq(() => gmb_gfx_sprite_set_priority(spriteId, priority));
    public void SpriteSetScale(int spriteId, float sx, float sy) => Enq(() => gmb_gfx_sprite_set_scale(spriteId, sx, sy));
    public void SpriteSetRotation(int spriteId, float degrees) => Enq(() => gmb_gfx_sprite_set_rotation(spriteId, degrees));
    public void SpriteSetTransform(int spriteId, long matrixId) => SpriteSetMatrix(spriteId, matrixId);
    public void SpriteSetMatrix(int spriteId, long matrixId) => Enq(() =>
    {
        var m = MatrixResolver?.Invoke(matrixId);
        gmb_gfx_sprite_set_matrix(spriteId, m ?? new[] { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 });
    });
    // Ultime collisioni rilevate (parità GraphicsManager.TakeLastCollisions per PCodeVM).
    private readonly System.Collections.Generic.List<SpriteCollisionHit> _lastCollisions = new();

    public double SpriteCollide(int id1, int id2)
    {
        var d = gmb_gfx_collide(id1, id2);
        if (d > 0) _lastCollisions.Add(new SpriteCollisionHit(id1, id2, 0, 0));
        return d;
    }
    public object[] SpriteCollideAll(int id)
    {
        var hits = new System.Collections.Generic.List<object>();
        var count = gmb_gfx_sprite_count();
        for (var other = 1; other < count; other++)
        {
            if (other == id) continue;
            var d = gmb_gfx_collide(id, other);
            if (d > 0)
            {
                hits.Add(new object[] { (double)id, (double)other, d });
                _lastCollisions.Add(new SpriteCollisionHit(id, other, 0, 0));
            }
        }
        return hits.ToArray();
    }
    public object[] SpriteCollideBatch(float[] xs, float[] ys, float w, float h)
    {
        var hits = new System.Collections.Generic.List<object>();
        var count = gmb_gfx_sprite_count();
        for (var i = 0; i < xs.Length; i++)
            for (var other = 1; other < count; other++)
            {
                var d = gmb_gfx_collide(i + 1, other);
                if (d > 0)
                {
                    hits.Add(new object[] { (double)(i + 1), (double)other, d });
                    _lastCollisions.Add(new SpriteCollisionHit(i + 1, other, 0, 0));
                }
            }
        return hits.ToArray();
    }
    public SpriteCollisionHit[] TakeLastCollisions()
    {
        // Collisioni rilevate dal motore C all'ultimo SYNC (ON SPRITE COLLISION): prima delle hit esplicite di COLLIDE.
        var list = new System.Collections.Generic.List<SpriteCollisionHit>();
        try
        {
            var i1 = new int[256]; var i2 = new int[256]; var px = new float[256]; var py = new float[256];
            var n = gmb_gfx_take_collisions(i1, i2, px, py, 256);
            for (var k = 0; k < n; k++) list.Add(new SpriteCollisionHit(i1[k], i2[k], px[k], py[k]));
        }
        catch { }
        list.AddRange(_lastCollisions);
        _lastCollisions.Clear();
        return list.ToArray();
    }
    public int[] SpriteGetPixels(int spriteId)
    {
        var buf = new int[4 + 4096];
        var n = gmb_gfx_sprite_pixels_to_array(spriteId, buf, buf.Length);
        return n > 0 ? buf : Array.Empty<int>();
    }

    // GPM (park-based): la completion scatta DOPO che il main thread ha eseguito
    // l'operazione (mai wait, mai completion fittizia). Per SYNC: dopo il present.
    public void SpriteLoadGpm(string filePath, Action<object?, Exception?> completion)
        => Enq(() => { try { completion(gmb_gfx_sprite_load(filePath), null); } catch (Exception ex) { completion(null, ex); } });
    public void LoadTextureGpm(string path, Action<object?, Exception?> completion)
        => Enq(() => { try { completion(gmb_gfx_load_texture(path), null); } catch (Exception ex) { completion(null, ex); } });
    public void LoadTextureSheetGpm(string path, int cellW, int cellH, Action<object?, Exception?> completion)
        => Enq(() =>
        {
            try
            {
                var id = gmb_gfx_load_texture_sheet(path, cellW, cellH);
                completion(id > 0 ? new[] { id } : Array.Empty<int>(), null);
            }
            catch (Exception ex) { completion(null, ex); }
        });
    public void SyncGpm(Action<object?, Exception?> completion)
    {
        Enq(() => gmb_gfx_sync());
        EnqAfterPresent(() => completion(null, null));
    }
    public void PointGpm(int x, int y, Action<object?, Exception?> completion)
        => Enq(() => { try { completion(gmb_gfx_point(x, y), null); } catch (Exception ex) { completion(null, ex); } });
    public void ScreenGpm(int mode, int width, int height, int bpp, Action<object?, Exception?> completion)
        => Enq(() =>
        {
            try { ScreenCore(mode, width, height, bpp); completion(null, null); }
            catch (Exception ex) { completion(null, ex); }
        });
    public void SpriteGetPixelsGpm(int spriteId, Action<object?, Exception?> completion)
        => Enq(() =>
        {
            try
            {
                var buf = new int[4 + 4096];
                var n = gmb_gfx_sprite_pixels_to_array(spriteId, buf, buf.Length);
                completion(n > 0 ? buf : Array.Empty<int>(), null);
            }
            catch (Exception ex) { completion(null, ex); }
        });

    // Tilemap (mappa su gmb_gfx_tilemap_*; un tilemap attivo per volta, come l'uso BASIC)
    public void TilemapLoad(int textureId, int tileW, int tileH, int tilesPerRow, int mapW, int mapH, int[] tiles) => Enq(() =>
    {
        _tilemapHandle = gmb_gfx_tilemap_from_texture(textureId, tileW, tileH, mapW, mapH);
        if (_tilemapHandle < 0 || tiles == null) return;
        for (var i = 0; i < tiles.Length; i++)
        {
            var col = i % mapW;
            var row = i / mapW;
            if (row >= mapH) break;
            gmb_gfx_tilemap_set(_tilemapHandle, col, row, tiles[i]);
        }
    });
    public void TilemapSet(int row, int col, int tileIndex) => Enq(() =>
    {
        if (_tilemapHandle >= 0) gmb_gfx_tilemap_set(_tilemapHandle, col, row, tileIndex);
    });
    public void TilemapSetOffset(float x, float y) => Enq(() =>
    {
        if (_tilemapHandle < 0) return;
        gmb_gfx_tilemap_offset(_tilemapHandle, (int)x - gmb_gfx_tilemap_offset_x(_tilemapHandle),
                                              (int)y - gmb_gfx_tilemap_offset_y(_tilemapHandle));
    });
    public void TilemapScroll(float dx, float dy) => Enq(() =>
    {
        if (_tilemapHandle >= 0) gmb_gfx_tilemap_scroll(_tilemapHandle, (int)dx, (int)dy);
    });
    public void TilemapDraw() => Enq(() =>
    {
        if (_tilemapHandle >= 0)
            gmb_gfx_tilemap_draw(_tilemapHandle, gmb_gfx_tilemap_offset_x(_tilemapHandle),
                                                 gmb_gfx_tilemap_offset_y(_tilemapHandle));
    });

    // Chart
    public long ChartCreate(string title, string xLabel, string yLabel) => gmb_gfx_chart_create();
    public void ChartPlot(long handle, double[] xs, double[] ys, string plotType, string legend)
        => Enq(() => gmb_gfx_chart_plot((int)handle, xs, ys, Math.Min(xs.Length, ys.Length), legend ?? ""));
    public void ChartFinancial(long handle, double[] ohlc, string plotType, string legend)
        => Enq(() => gmb_gfx_chart_plot_financial((int)handle, ohlc, ohlc.Length, plotType ?? "CANDLESTICK", legend ?? ""));
    public void ChartConfigSet(int handle, string prop, double num, string str) => Enq(() => gmb_gfx_chart_set(handle, prop ?? "", num, str ?? ""));
    public double ChartConfigGet(int handle, string prop) { try { return gmb_gfx_chart_get(handle, prop ?? ""); } catch { return 0; } }
    public string ChartConfigGetStr(int handle, string prop) { try { return System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gmb_gfx_chart_get_str(handle, prop ?? "")) ?? ""; } catch { return ""; } }
    public double ChartDraw(int handle, string op, double[] args) { try { return gmb_gfx_chart_draw(handle, op ?? "", args ?? System.Array.Empty<double>(), args?.Length ?? 0); } catch { return 0; } }
    public void ChartShow(long handle, int x, int y) => Enq(() => gmb_gfx_chart_show((int)handle, x, y));
    public void ChartClear(long handle) => Enq(() => gmb_gfx_chart_clear((int)handle));
    public void ChartClose(long handle) => Enq(() => gmb_gfx_chart_close((int)handle));
    public void ChartSave(long handle, string path) => Enq(() => gmb_gfx_chart_save((int)handle, path));

    // TUI su grafica: griglia di celle (char+ARGB) renderizzata dal motore C.
    public void RenderTui(TuiCell[] cells, int w, int h)
    {
        if (cells == null || w <= 0 || h <= 0) return;
        var chars = new int[cells.Length];
        var fg = new uint[cells.Length];
        var bg = new uint[cells.Length];
        for (var i = 0; i < cells.Length; i++)
        {
            chars[i] = cells[i].Character;
            fg[i] = cells[i].FgArgb;
            bg[i] = cells[i].BgArgb;
        }
        Enq(() => gmb_gfx_render_cells(chars, fg, bg, w, h));
    }
}
