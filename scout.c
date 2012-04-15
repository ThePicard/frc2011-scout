#include <gtk/gtk.h>
#include <glib.h>
#include <sqlite3.h>

#define MY_INSERT_SQL_LEN 145
#define MY_TEAMS_SQL_LEN 37
#define MY_REFRESH_SQL_LEN 105
static char *my_insert_sql = "INSERT INTO matches (matchno, teamno, autonomous, high, middle, low, miniplace, penalties, cards, comment) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
static char *my_setup_sql = "CREATE TABLE matches(matchno INTEGER, teamno INTEGER, autonomous INTEGER, high INTEGER, middle INTEGER, low INTEGER, miniplace INTEGER, penalties INTEGER, cards INTEGER, comment TEXT);";
static char *my_teams_sql = "SELECT DISTINCT teamno FROM matches;";
static char *my_refresh_sql = "SELECT autonomous, high, middle, low, miniplace, penalties, cards FROM matches WHERE teamno = ?";

enum {
	COL_TEAMNO = 0,
	COL_AUTO,
	COL_HIGH,
	COL_MID,
	COL_LOW,
	COL_MINI,
	COL_PLACE,
	COL_PEN,
	COL_RED,
	COL_YEL,
	NUM_COLS
};

struct db_t {
	sqlite3 *handle;
	sqlite3_stmt *insert, *refresh, *teams;
} db;

struct entry_t {
	GtkWidget *window;
	GtkEntry *match, *team;
	GtkToggleButton *autohigh, *automiddle, *autolow, *autonone, *red, *yellow, *nocard;
	GtkSpinButton *penalties, *minibot, *high, *middle, *low;
	GtkTextBuffer *comment;
} entry;

struct viewer_t {
	GtkWidget *window, *view, *scroll;
} viewer;

void my_db_perror(gchar *location) {
	g_printf("%s: %s\n", location, sqlite3_errmsg(db.handle));
}

gboolean my_db_open (char *f) {
	if (db.handle != NULL) {
		sqlite3_finalize(db.insert);
		sqlite3_close(db.handle);
	}
	if (sqlite3_open(f, &db.handle) == SQLITE_OK) {
		gchar *name;
		gchar *title;
		name = g_path_get_basename(f);
		title = g_strconcat("Data Entry: ", name, NULL);
		gtk_window_set_title(GTK_WINDOW(entry.window), title);
		g_free(name);
		g_free(title);
		return TRUE;
	}
	my_db_perror("open");
	sqlite3_close(db.handle);
	db.handle = NULL;
	gtk_window_set_title(GTK_WINDOW(entry.window), "Data Entry: No Database");
	return FALSE;
}

void my_view_refresh() {
	GtkListStore *store;
	GtkTreeIter iter;
	int rc1, rc2;
	if (db.handle == NULL) return;
	if (db.refresh == NULL) {
		if (sqlite3_prepare_v2(db.handle, my_refresh_sql, MY_REFRESH_SQL_LEN, &db.refresh, NULL) != SQLITE_OK) {
			my_db_perror("prepare");
			return;
		}
	}
	if (db.teams == NULL) {
		if (sqlite3_prepare_v2(db.handle, my_teams_sql, MY_TEAMS_SQL_LEN, &db.teams, NULL) != SQLITE_OK) {
			my_db_perror("prepare");
			return;
		}
	} else if (sqlite3_reset(db.teams) != SQLITE_OK) {
		my_db_perror("reset");
		return;
	}
	store = gtk_list_store_new(NUM_COLS, G_TYPE_UINT, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_UINT, G_TYPE_UINT);
	while ((rc1 = sqlite3_step(db.teams)) == SQLITE_ROW) {
		int teamno = sqlite3_column_int(db.teams, 0);
		if (sqlite3_reset(db.refresh) != SQLITE_OK) {
			my_db_perror("reset");
			return;
		}
		sqlite3_bind_int(db.refresh, 1, teamno);
		int reps = 0, autogpa = 0, high = 0, middle = 0, low = 0, miniplace = 0, avgplace = 0, miniattemps = 0, penalties = 0, cards = 0, reds = 0, yels = 0;
		while((rc2 = sqlite3_step(db.refresh)) == SQLITE_ROW) {
			autogpa += sqlite3_column_int(db.refresh, 0);
			high += sqlite3_column_int(db.refresh, 1);
			middle += sqlite3_column_int(db.refresh, 2);
			low += sqlite3_column_int(db.refresh, 3);
			miniplace = sqlite3_column_int(db.refresh, 4);
			if (miniplace > 0) {miniattemps++; avgplace += miniplace;}
			penalties += sqlite3_column_int(db.refresh, 5);
			cards = sqlite3_column_int(db.refresh, 6);
			if (cards == 2) reds++; else if (cards == 1) yels++;
			reps++;
		}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
			COL_TEAMNO, (guint) teamno,
			COL_AUTO, (gdouble) autogpa / (gdouble) reps,
			COL_HIGH, (gdouble) high / (gdouble) reps,
			COL_MID, (gdouble) middle / (gdouble) reps,
			COL_LOW, (gdouble) low / (gdouble) reps,
			COL_MINI, (gdouble) miniattemps / (gdouble) reps,
			COL_PLACE, (gdouble) avgplace / (gdouble) miniattemps,
			COL_PEN, (gdouble) penalties / (gdouble) reps,
			COL_RED, (guint) reds,
			COL_YEL, (guint) yels,
			-1);
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(viewer.view), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

int main (int argc, char **argv) {
	GtkBuilder *builder;
	GtkCellRenderer *renderer;

	gtk_init(&argc, &argv);

	db.handle = NULL;
	db.insert = NULL;

	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "scout.glade", NULL);

	entry.window = GTK_WIDGET(gtk_builder_get_object(builder, "insert_window"));
	entry.match = GTK_ENTRY(gtk_builder_get_object(builder, "match_entry"));
	entry.team = GTK_ENTRY(gtk_builder_get_object(builder, "team_entry"));
	entry.autohigh = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "auto_high_radiobutton"));
	entry.automiddle = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "auto_middle_radiobutton"));
	entry.autolow = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "auto_low_radiobutton"));
	entry.autonone = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "auto_none_radiobutton"));
	entry.red = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "pen_red_radiobutton"));
	entry.yellow = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "pen_yellow_radiobutton"));
	entry.nocard = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "pen_none_radiobutton"));
	entry.penalties = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "pen_penalties_spinbutton"));
	entry.minibot = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "minibot_spinbutton"));
	entry.high = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "tube_high_spinbutton"));
	entry.middle = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "tube_middle_spinbutton"));
	entry.low = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "tube_low_spinbutton"));
	entry.comment = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(builder, "comment_textview")));

	viewer.window = GTK_WIDGET(gtk_builder_get_object(builder, "viewer_window"));
	viewer.scroll = GTK_WIDGET(gtk_builder_get_object(builder, "scrolledwindow"));

	gtk_builder_connect_signals(builder, NULL);
	g_object_unref(G_OBJECT(builder));

	viewer.view = gtk_tree_view_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
	       -1,
	       "Team",
	       renderer,
	       "text", COL_TEAMNO,
	       NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
	       -1,
	       "Auto",
	       renderer,
	       "text", COL_AUTO,
	       NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "High",
               renderer,
               "text", COL_HIGH,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Middle",
               renderer,
               "text", COL_MID,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Low",
               renderer,
               "text", COL_LOW,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Mini %",
               renderer,
               "text", COL_MINI,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Mini Place",
               renderer,
               "text", COL_PLACE,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Penalties",
               renderer,
               "text", COL_PEN,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Reds",
               renderer,
               "text", COL_RED,
               NULL);

        renderer = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (viewer.view),
               -1,
               "Yellows",
               renderer,
               "text", COL_YEL,
               NULL);

	gtk_container_add(GTK_CONTAINER(viewer.scroll), viewer.view);

	gtk_widget_show(entry.window);
	gtk_main();
	return 0;
}

void file_new_menuitem_activate_cb(GtkMenuItem *source, gpointer data) {
	GtkWidget *fc;
	fc = gtk_file_chooser_dialog_new(
		"New Database",
		GTK_WINDOW(entry.window),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Save", GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		gboolean openrc;
		gchar *filename;
		char *errmsg = NULL;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		if (filename == NULL) return;
		if (g_file_test(filename, G_FILE_TEST_EXISTS)) g_remove(filename);
		openrc = my_db_open(filename);
		g_free(filename);
		if (!openrc) return;
		if (sqlite3_exec(db.handle, my_setup_sql, NULL, NULL, &errmsg) != SQLITE_OK) {
			g_printf("%s\n", errmsg);
			sqlite3_free(errmsg);
			sqlite3_close(db.handle);
			db.handle = NULL;
			return;
		}
	}
	gtk_widget_destroy(fc);
}

void file_open_menuitem_activate_cb(GtkMenuItem *source, gpointer data) {
	GtkWidget *fc;
	fc = gtk_file_chooser_dialog_new(
		"Open Database",
		GTK_WINDOW(entry.window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Open", GTK_RESPONSE_ACCEPT,
		NULL);
	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		if (g_file_test(filename, G_FILE_TEST_EXISTS)) my_db_open(filename);
		g_free(filename);
	}
	gtk_widget_destroy(fc);
}
	
void file_save_menuitem_activate_cb(GtkMenuItem *source, gpointer data) {
	if (db.handle == NULL) return;
	if (db.insert == NULL) {
		if (sqlite3_prepare_v2(db.handle, my_insert_sql, MY_INSERT_SQL_LEN, &db.insert, NULL) != SQLITE_OK) {
			my_db_perror("prepare");
			return;
		}
	} else if (sqlite3_reset(db.insert) != SQLITE_OK) {
		my_db_perror("reset");
		return;
	}

	gint matchno, teamno, autonomous, high, middle, low, miniplace, penalties, cards;
	gchar *comment;
	GtkTextIter commentstart, commentend;

	matchno = (gint) g_ascii_strtoll(gtk_entry_get_text(entry.match), (gchar**) NULL, 10);
	teamno = (gint) g_ascii_strtoll(gtk_entry_get_text(entry.team), (gchar**) NULL, 10);

	if (gtk_toggle_button_get_active(entry.autohigh)) autonomous = 3;
	else if (gtk_toggle_button_get_active(entry.automiddle)) autonomous = 2;
	else if (gtk_toggle_button_get_active(entry.autolow)) autonomous = 1;
	else if (gtk_toggle_button_get_active(entry.autonone)) autonomous = 0;
	// else error

	high = gtk_spin_button_get_value_as_int(entry.high);
	middle = gtk_spin_button_get_value_as_int(entry.middle);
	low = gtk_spin_button_get_value_as_int(entry.low);
	miniplace = gtk_spin_button_get_value_as_int(entry.minibot);
	penalties = gtk_spin_button_get_value_as_int(entry.penalties);

	if (gtk_toggle_button_get_active(entry.red)) cards = 2;
	else if (gtk_toggle_button_get_active(entry.yellow)) cards = 1;
	else if (gtk_toggle_button_get_active(entry.nocard)) cards = 0;
	// else error

	gtk_text_buffer_get_bounds(entry.comment, &commentstart, &commentend);
	comment = gtk_text_iter_get_visible_text(&commentstart, &commentend);

	sqlite3_bind_int(db.insert, 1, matchno);
	sqlite3_bind_int(db.insert, 2, teamno);
	sqlite3_bind_int(db.insert, 3, autonomous);
	sqlite3_bind_int(db.insert, 4, high);
	sqlite3_bind_int(db.insert, 5, middle);
	sqlite3_bind_int(db.insert, 6, low);
	sqlite3_bind_int(db.insert, 7, miniplace);
	sqlite3_bind_int(db.insert, 8, penalties);
	sqlite3_bind_int(db.insert, 9, cards);
	sqlite3_bind_text(db.insert, 10, comment, -1, SQLITE_TRANSIENT);
	g_free(comment);

	if (sqlite3_step(db.insert) != SQLITE_DONE) {
		my_db_perror("insert step");
		return;
	}
	//g_printf("Match Number: %d\nTeam Number: %d\nAutonomous: %d\nHigh Tubes: %d\nMiddle Tubes: %d\nLow Tubes: %d\nMinibot Place: %d\nPenalties: %d\nCards: %d\nComments: %s\n", matchno, teamno, autonomous, high, middle, low, miniplace, penalties, cards, comment);

	//gtk_entry_set_text(entry.match, "");
	gtk_entry_set_text(entry.team, "");

	gtk_toggle_button_set_active(entry.autonone, TRUE);

	gtk_spin_button_set_value(entry.high, 0.0);
	gtk_spin_button_set_value(entry.middle, 0.0);
	gtk_spin_button_set_value(entry.low, 0.0);
	gtk_spin_button_set_value(entry.minibot, 0.0);
	gtk_spin_button_set_value(entry.penalties, 0.0);

	gtk_toggle_button_set_active(entry.nocard, TRUE);

	gtk_text_buffer_set_text(entry.comment, "", -1);
}

void view_list_menuitem_activate_cb(GtkMenuItem *source, gpointer data) {
	my_view_refresh();
	gtk_widget_show_all(viewer.window);
}

void file_refresh_menuitem1_activate_cb(GtkMenuItem *source, gpointer data) {
	my_view_refresh();
}
