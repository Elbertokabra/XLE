// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;

namespace GUILayer 
{
	public ref class EngineControl : public System::Windows::Forms::UserControl
	{
	public:
		EngineControl();

	protected:
		~EngineControl();

        virtual void OnPaint(PaintEventArgs^) override;
        virtual void OnPaintBackground(PaintEventArgs^) override;

	private:
		System::ComponentModel::Container ^components;

        ref class Pimpl;
        Pimpl^ _pimpl;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
		}
#pragma endregion
	};
}

