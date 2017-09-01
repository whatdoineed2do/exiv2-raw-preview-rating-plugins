#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

extern "C" {

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog_plugin_exiv2_ratings.h"

#include <gmodule.h>

#include <libpeas/peas.h>

#include <eog/eog-application.h>
#include <eog/eog-debug.h>
#include <eog/eog-thumb-view.h>
#include <eog/eog-window.h>
#include <eog/eog-window-activatable.h>
}


#include <string>
#include <list>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

#include <exiv2/exiv2.hpp>


enum {
  PROP_0,
  PROP_WINDOW
};

#define EOG_EXIV_XMP_RATING_PLUGIN_MENU_ID  "EogPluginRunExiv2Rating"
#define EOG_EXIV_XMP_RATING_PLUGIN_ACTION   "exiv2rating"
#define EOG_EXIV_XMP_RATING_PLUGIN_ACTION_U "exiv2rating_u"

static void eog_window_activatable_iface_init (EogWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EogExiv2RatingPlugin,
                                eog_exiv2_ratings_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (EOG_TYPE_WINDOW_ACTIVATABLE,
                                                               eog_window_activatable_iface_init))

using namespace  std;

static
ostream&  operator<<(ostream& os_, const Exiv2::XmpData& data_) 
{
    for (Exiv2::XmpData::const_iterator md = data_.begin();
         md != data_.end(); ++md) 
    {
#if 0
        os_ << std::setfill(' ') << std::left << std::setw(44)
            << md->key() << " "
            << std::setw(9) << std::setfill(' ') << std::left
            << md->typeName() << " " << std::dec << std::setw(3)
            << std::setfill(' ') << std::right
            << md->count() << "  " << std::dec << md->
            value() << std::endl;
#else
        os_ << "{ " << md->key() << " " << md->typeName() << " " << md->count() << " " << md->value() << " }";
#endif
    }

    return os_;
}


class _ExifProxy
{
  public:
    friend ostream&  operator<<(ostream&, const _ExifProxy&);

    struct HistoryEvnt {
        HistoryEvnt(const string& f_, const Exiv2::ExifData& exif_, bool rated_)
            : f(f_), exif(exif_), rated(rated_)
        { }

        HistoryEvnt(const string& f_, const Exiv2::ExifData* exif_ = NULL, bool rated_=false)
            : f(f_), exif(exif_ == NULL ? Exiv2::ExifData() : *exif_), rated(rated_)
        { }

        const string           f;
        const Exiv2::ExifData  exif;
        const bool             rated;
    };

    typedef list<HistoryEvnt>  History;


    _ExifProxy() : _xmp(NULL), _xmpkpos(NULL), _mtime(0)
    { }

    _ExifProxy&  ref(EogThumbView& ev_)
    {
        return ref(*eog_thumb_view_get_first_selected_image(&ev_));
    }

    _ExifProxy&  ref(EogWindow& ew_)
    {
        return ref( *eog_window_get_image(&ew_) );
    }

    _ExifProxy&  ref(EogImage& ei_)
    {
        GFile*  f = eog_image_get_file(&ei_ );
        if ( f == NULL) {
            _clear();
            return *this;
        }

        char* const  fpath = g_file_get_path(f);
        if (strcmp(fpath, _file.c_str()) == 0) {
            g_free(fpath);
            return *this;
        }
        struct stat  st;
        memset(&st, 0, sizeof(st));
        if (stat(fpath, &st) < 0) {
            _clear();
            g_free(fpath);
            return *this;
        }

        _clear();
        _mtime = st.st_mtime;

        try
        {
            _img = Exiv2::ImageFactory::open(fpath);
            _img->readMetadata();
            //_xmpkpos = NULL;

            _file = fpath;

            Exiv2::XmpData&  xmpData = _img->xmpData();
            Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(_ExifProxy::_XMPKEY));
            if (kpos != xmpData.end()) {
                _xmp = &xmpData;
                _xmpkpos = kpos;
            }
        }
        catch(Exiv2::AnyError & e) {
            cerr << fpath << ": failed to set XMP rating - " << e << endl;
        }
        g_free(fpath);
        
        return *this;
    }

    bool  valid() const
    {
        return _img.get() != 0;
    }

    bool  rated() const
    {
        return _xmp == NULL ? false : _xmpkpos != _xmp->end();
    }

    const char*  rating()
    {
        static const string  DEFLT_RATING = "XMP Rating: -----";
        _rating = DEFLT_RATING;

        if (rated())
        {
            // spec say -1..5
            long  N = _xmpkpos->toLong();
            if (N < 0) {
            }
            else
            {
                if (N > 5) {
                    N = 5;
                }
                char*  n = ((char*)_rating.c_str())+12;
                for (int i=0; i<N; i++) {
                    n[i] = '*';
                }
            }
        }
        return _rating.c_str();
    }

    /* mark the image if its not already rated
     */
    bool  fliprating()
    {
        bool  flipped = false;
        if (_img.get() == 0) {
            // something went wrong earlier/no img ... do nothing
            return flipped;
        }

        try
        {
            bool  r = false;
            if (_xmp == NULL)
            {
                // previously found no XMP data set
                _xmp = &_img->xmpData();

                (*_xmp)[_ExifProxy::_XMPKEY] = _ExifProxy::_XMPVAL;
                _xmpkpos = _xmp->findKey(Exiv2::XmpKey(_ExifProxy::_XMPKEY));

                r = true;
            }
            else
            {
                if (_xmpkpos == _xmp->end()) {
                    (*_xmp)[_ExifProxy::_XMPKEY] = _ExifProxy::_XMPVAL;
                    _xmpkpos = _xmp->findKey(Exiv2::XmpKey(_ExifProxy::_XMPKEY));

                    r = true;
                }
                else {
                    _xmp->erase(_xmpkpos);
                    _xmpkpos = _xmp->end();
                    r = false;
                }
            }
            _img->writeMetadata();

            flipped = true;

            struct utimbuf  tmput;
            memset(&tmput, 0, sizeof(tmput));
            tmput.modtime = _mtime;
            utime(_file.c_str(), &tmput);

            _ExifProxy::History::iterator  h;
            for (h=_history.begin(); h!=_history.end(); ++h)
            {
                if (h->f == _file) {
                    break;
                }
            }
            if (h != _history.end()) {
                _history.erase(h);
            }
            else
            {
                _history.push_back(_ExifProxy::HistoryEvnt(_file, _img->exifData(), r));
            }
        }
        catch (const exception& ex)
        {
            cerr << _file << ": failed to update XMP rating - " << ex.what() << endl;
        }
        return flipped;
    }


    const string&  file() const
    { return _file; }

    const _ExifProxy::History&  history() const
    { return _history; }


  private:
    _ExifProxy(const _ExifProxy&);
    void operator=(const _ExifProxy&);

    static const string               _XMPKEY;
    static const Exiv2::XmpTextValue  _XMPVAL;


    time_t  _mtime;

    Exiv2::Image::AutoPtr  _img;
    Exiv2::XmpData*  _xmp;
    Exiv2::XmpData::iterator  _xmpkpos;

    string  _file;
    string  _rating;


    void  _clear()
    {
        _xmp = NULL;
        _file.clear();
        _img.reset();
        _mtime = 0;
        _rating.clear();
    }

    _ExifProxy::History  _history;
};

const string               _ExifProxy::_XMPKEY = "Xmp.xmp.Rating";
const Exiv2::XmpTextValue  _ExifProxy::_XMPVAL = Exiv2::XmpTextValue("5");


ostream&  operator<<(ostream& os_, const _ExifProxy& obj_)
{
    os_ << obj_._file;
    if (obj_._xmp == NULL) {
        return os_ << " [ <nil> ]";
    }
    return os_ << " [ " << *obj_._xmp << " ]";
}

ostream&  operator<<(ostream& os_, const _ExifProxy::HistoryEvnt& obj_)
{
    struct _ETag {
        const Exiv2::ExifKey  tag;
        const char*  prfx;

        _ETag(const Exiv2::ExifKey&  tag_, const char* prfx_) : tag(tag_), prfx(prfx_) { }
    };
    static const _ETag etags[] = {
        _ETag(Exiv2::ExifKey("Exif.Image.Model"), ""),
        _ETag(Exiv2::ExifKey("Exif.Image.DateTime"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ExposureTime"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.FNumber"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"), "ISO")
    };

    os_ << obj_.f << ": " << (obj_.rated ? "R" : "U");

    for (int i=0; i<5; ++i) {
        Exiv2::ExifData::const_iterator  e = obj_.exif.findKey(etags[i].tag);
        if (e == obj_.exif.end()) {
            continue;
        }

        os_ << " " << etags[i].prfx << *e;
    }

    return os_;
}


static void
_upd_statusbar_exif(GtkStatusbar* statusbar_,
                    const Exiv2::ExifData* exif_, const char* msg_)
{
    gtk_statusbar_pop(statusbar_, 0);
    if (msg_ == NULL) {
        gtk_widget_hide(GTK_WIDGET(statusbar_));
        return;
    }
    gtk_statusbar_push(statusbar_, 0, msg_);
    gtk_widget_show(GTK_WIDGET(statusbar_));
}


static void
_exiv2_rating_setunset(GSimpleAction *simple, GVariant *parameter, gpointer user_data, const bool set_)
{
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (user_data);

    _upd_statusbar_exif(GTK_STATUSBAR(plugin->statusbar_exif),
                        NULL,
                        plugin->exifproxy->fliprating() ? plugin->exifproxy->rating() : "XMP Rating: -/-");
}

static void
_exiv2_rating_set_cb(GSimpleAction *simple, GVariant      *parameter, gpointer       user_data)
{
    _exiv2_rating_setunset(simple, parameter, user_data, true);
}

static void
_exiv2_rating_unset_cb(GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
    _exiv2_rating_setunset(simple, parameter, user_data, false);
}


static void
eog_exiv2_ratings_plugin_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		plugin->window = EOG_WINDOW (g_value_dup_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
eog_exiv2_ratings_plugin_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		g_value_set_object (value, plugin->window);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
eog_exiv2_ratings_plugin_init (EogExiv2RatingPlugin *plugin)
{
	eog_debug_message (DEBUG_PLUGINS, "EogExiv2RatingPlugin initializing");
        plugin->exifproxy = new _ExifProxy();
}

static void
eog_exiv2_ratings_plugin_dispose (GObject *object)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

	eog_debug_message (DEBUG_PLUGINS, "EogExiv2RatingPlugin disposing");

	if (plugin->window != NULL) 
        {
            g_object_unref (plugin->window);
            plugin->window = NULL;

            if (plugin->exifproxy) {
                const _ExifProxy::History&  h = plugin->exifproxy->history();
                for ( _ExifProxy::History::const_iterator i=h.begin(); i!=h.end(); ++i)
                {
                    cout << *i << endl;
                }
            }
            delete plugin->exifproxy;
            plugin->exifproxy = NULL;
	}

	G_OBJECT_CLASS (eog_exiv2_ratings_plugin_parent_class)->dispose (object);
}

static void
eog_exiv2_ratings_plugin_update_action_state (EogExiv2RatingPlugin *plugin, EogThumbView *view)
{
    GAction *action;
    EogThumbView *thumbview;
    gboolean enable = FALSE;

    thumbview = view == NULL ? EOG_THUMB_VIEW (eog_window_get_thumb_view (plugin->window)) : view;

    if (G_LIKELY (thumbview))
    {
        enable = (eog_thumb_view_get_n_selected (thumbview) != 0);
        //enable = (eog_thumb_view_get_first_selected_image(thumbview) != 0);
        if (enable) {
            //plugin->exifproxy->ref(*plugin->window);  <-- this is the prev file before the change
            plugin->exifproxy->ref(*thumbview);
            //cout << *plugin->exifproxy << endl; 
            _upd_statusbar_exif(GTK_STATUSBAR(plugin->statusbar_exif),
                                NULL,
                                plugin->exifproxy->valid() ? 
                                    plugin->exifproxy->rating() : "-/-");
        }
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (plugin->window),
                                         EOG_EXIV_XMP_RATING_PLUGIN_ACTION);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enable);
}

static void
_selection_changed_cb (EogThumbView *thumbview, gpointer user_data)
{
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (user_data);

    if (G_LIKELY (plugin)) {
        eog_exiv2_ratings_plugin_update_action_state (plugin, thumbview);
    }
}

static void
eog_exiv2_ratings_plugin_activate (EogWindowActivatable *activatable)
{
    const gchar * const accel_keys[] = { "R", NULL };
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (activatable);
    GMenu *model, *menu;
    GMenuItem *item;
    GSimpleAction *action;


    GtkWidget *statusbar = eog_window_get_statusbar(plugin->window);
    plugin->statusbar_exif = gtk_statusbar_new();

    gint  minh, nath;
    gtk_widget_get_preferred_height(plugin->statusbar_exif, &minh, &nath);
    gint  minw, natw;
    gtk_widget_get_preferred_width(plugin->statusbar_exif, &minw, &natw);

    gtk_widget_set_size_request (plugin->statusbar_exif, natw, nath);
    gtk_box_pack_end (GTK_BOX(statusbar), plugin->statusbar_exif, FALSE, FALSE, 0);


    eog_debug (DEBUG_PLUGINS);

    model= eog_window_get_gear_menu_section (plugin->window,
                                             "plugins-section");

    g_return_if_fail (G_IS_MENU (model));

    /* Setup and inject action */
    action = g_simple_action_new (EOG_EXIV_XMP_RATING_PLUGIN_ACTION, NULL);
#ifndef MULTI_CB
    g_signal_connect(action, "activate", G_CALLBACK (_exiv2_rating_set_cb), plugin);
    g_action_map_add_action (G_ACTION_MAP (plugin->window), G_ACTION (action));
#else
    g_action_map_add_action_entries(G_ACTION_MAP(plugin->window),
                                    actions, G_N_ELEMENTS(actions), plugin->window);
#endif

    g_object_unref (action);

    g_signal_connect (G_OBJECT (eog_window_get_thumb_view (plugin->window)),
                      "selection-changed",
                      G_CALLBACK (_selection_changed_cb),
                      plugin);
    eog_exiv2_ratings_plugin_update_action_state (plugin, EOG_THUMB_VIEW (eog_window_get_thumb_view (plugin->window)));

#if 0
    /* Append entry to the window's gear menu */
    menu = g_menu_new ();
    g_menu_append (menu, "Add Exif Rating",
                   "win." EOG_EXIV_XMP_RATING_PLUGIN_ACTION);

    item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
    g_menu_item_set_attribute (item, "id",
                               "s", EOG_EXIV_XMP_RATING_PLUGIN_MENU_ID);
    g_menu_item_set_attribute (item, G_MENU_ATTRIBUTE_ICON,
                               "s", "view-refresh-symbolic");
    g_menu_append_item (model, item);
    g_object_unref (item);

    g_object_unref (menu);
#endif

    /* Define accelerator keys */
    gtk_application_set_accels_for_action (GTK_APPLICATION (EOG_APP),
                                           "win." EOG_EXIV_XMP_RATING_PLUGIN_ACTION,
                                           accel_keys);
}

static void
eog_exiv2_ratings_plugin_deactivate (EogWindowActivatable *activatable)
{
    const gchar * const empty_accels[1] = { NULL };
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (activatable);
    GMenu *menu;
    GMenuModel *model;
    gint i;

    eog_debug (DEBUG_PLUGINS);

#if 0
    menu = eog_window_get_gear_menu_section (plugin->window,
                                             "plugins-section");

    g_return_if_fail (G_IS_MENU (menu));

    /* Remove menu entry */
    model = G_MENU_MODEL (menu);
    for (i = 0; i < g_menu_model_get_n_items (model); i++) {
            gchar *id;
            if (g_menu_model_get_item_attribute (model, i, "id", "s", &id)) {
                    const gboolean found =
                            (g_strcmp0 (id, EOG_EXIV_XMP_RATING_PLUGIN_MENU_ID) == 0);
                    g_free (id);

                    if (found) {
                            g_menu_remove (menu, i);
                            break;
                    }
            }
    }
#endif

    /* Unset accelerator */
    gtk_application_set_accels_for_action(GTK_APPLICATION (EOG_APP),
                                          "win." EOG_EXIV_XMP_RATING_PLUGIN_ACTION,
                                          empty_accels);

    /* Disconnect selection-changed handler as the thumbview would
     * otherwise still cause callbacks during its own disposal */
    g_signal_handlers_disconnect_by_func (eog_window_get_thumb_view (plugin->window),
                                          _selection_changed_cb, plugin);

    /* Finally remove action */
    g_action_map_remove_action (G_ACTION_MAP (plugin->window),
                                EOG_EXIV_XMP_RATING_PLUGIN_ACTION);

    GtkWidget *statusbar = eog_window_get_statusbar (plugin->window);
    gtk_container_remove(GTK_CONTAINER (statusbar), plugin->statusbar_exif);
}

static void
eog_exiv2_ratings_plugin_class_init (EogExiv2RatingPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose= eog_exiv2_ratings_plugin_dispose;
	object_class->set_property = eog_exiv2_ratings_plugin_set_property;
	object_class->get_property = eog_exiv2_ratings_plugin_get_property;

	g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
eog_exiv2_ratings_plugin_class_finalize (EogExiv2RatingPluginClass *klass)
{
}

static void
eog_window_activatable_iface_init (EogWindowActivatableInterface *iface)
{
	iface->activate = eog_exiv2_ratings_plugin_activate;
	iface->deactivate = eog_exiv2_ratings_plugin_deactivate;
}

extern "C" 
{
G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	eog_exiv2_ratings_plugin_register_type (G_TYPE_MODULE (module));
	peas_object_module_register_extension_type (module,
						    EOG_TYPE_WINDOW_ACTIVATABLE,
						    EOG_TYPE_EXIV_XMP_RATING_PLUGIN);
}
}
