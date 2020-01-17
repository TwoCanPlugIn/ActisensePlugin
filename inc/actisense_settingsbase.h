///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Nov  9 2019)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/choice.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/checklst.h>
#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/stattext.h>
#include <wx/grid.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/statbmp.h>
#include <wx/notebook.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class ActisenseSettingsBase
///////////////////////////////////////////////////////////////////////////////
class ActisenseSettingsBase : public wxDialog
{
	private:

	protected:
		wxNotebook* notebookTabs;
		wxPanel* panelSettings;
		wxPanel* panelNetwork;
		wxStaticText* lblNetwork;
		wxPanel* panelDevice;
		wxCheckBox* chkDeviceMode;
		wxCheckBox* chkEnableHeartbeat;
		wxCheckBox* chkGateway;
		wxCheckBox* chkSignalK;
		wxStaticText* labelNetworkAddress;
		wxStaticText* labelUniqueId;
		wxStaticText* labelManufacturer;
		wxStaticText* labelModelId;
		wxStaticText* labelSoftwareVersion;
		wxStaticText* labelDevice;
		wxStaticText* labelFunction;
		wxPanel* panelLogging;
		wxChoice* cmbLogging;
		wxCheckBox* chkSpreadsheet;
		wxCheckBox* chkInfluxDB;
		wxPanel* panelDebug;
		wxStaticText* labelDebug;
		wxButton* btnPause;
		wxButton* btnCopy;
		wxPanel* panelAbout;
		wxStaticBitmap* bmpAbout;
		wxStaticText* txtAbout;
		wxButton* btnOK;
		wxButton* btnApply;
		wxButton* btnCancel;

		// Virtual event handlers, overide them in your derived class
		virtual void OnInit( wxInitDialogEvent& event ) { event.Skip(); }
		virtual void OnChoiceInterfaces( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckPGN( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRightCick( wxMouseEvent& event ) { event.Skip(); }
		virtual void OnCheckMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckHeartbeat( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckGateway( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckSignalK( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnChoiceLogging( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckExcel( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCheckInfluxDb( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPause( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCopy( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnApply( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCancel( wxCommandEvent& event ) { event.Skip(); }


	public:
		wxChoice* cmbInterfaces;
		wxCheckListBox* chkListPGN;
		wxGrid* dataGridNetwork;
		wxTextCtrl* txtDebug;

		ActisenseSettingsBase( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Preferences"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 411,487 ), long style = wxDEFAULT_DIALOG_STYLE );
		~ActisenseSettingsBase();

};

