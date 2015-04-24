﻿

#pragma once
//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------

namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Media {
                namespace Animation {
                    ref class Storyboard;
                    ref class DoubleAnimation;
                }
            }
        }
    }
}
namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Controls {
                ref class Grid;
                ref class Frame;
                ref class Button;
                ref class StackPanel;
                ref class Border;
                ref class TextBlock;
                ref class CheckBox;
            }
        }
    }
}
namespace Windows {
    namespace UI {
        namespace Xaml {
            namespace Shapes {
                ref class Path;
            }
        }
    }
}

namespace JPG_Spinner
{
    partial ref class MainPage : public ::Windows::UI::Xaml::Controls::Page, 
        public ::Windows::UI::Xaml::Markup::IComponentConnector
    {
    public:
        void InitializeComponent();
        virtual void Connect(int connectionId, ::Platform::Object^ target);
    
    private:
        bool _contentLoaded;
    
        private: ::Windows::UI::Xaml::Media::Animation::Storyboard^ SpinLogo;
        private: ::Windows::UI::Xaml::Media::Animation::DoubleAnimation^ SpinLogoAnimation;
        private: ::Windows::UI::Xaml::Controls::Grid^ LeftPane;
        private: ::Windows::UI::Xaml::Controls::Frame^ ScenarioFrame;
        private: ::Windows::UI::Xaml::Controls::Button^ ButtonSelectFiles;
        private: ::Windows::UI::Xaml::Controls::StackPanel^ FooterPanel;
        private: ::Windows::UI::Xaml::Controls::Border^ DebugBorder;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ DebugText;
        private: ::Windows::UI::Xaml::Controls::CheckBox^ CheckBoxCrop;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ TextBlockCrop;
        private: ::Windows::UI::Xaml::Controls::CheckBox^ CheckBoxProgressive;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ TextBlockProgressive;
        private: ::Windows::UI::Xaml::Shapes::Path^ Logo;
        private: ::Windows::UI::Xaml::Controls::TextBlock^ AppTitle;
    };
}

