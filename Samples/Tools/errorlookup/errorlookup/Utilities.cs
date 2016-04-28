using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace errorlookup
{
    static class Utilities
    {
        /// <summary>
        /// Set up a text box to contain information when it does not have more meaningful text typed into it
        /// </summary>
        /// <param name="textBox">
        /// The TextBox control to add label functionality to
        /// </param>
        public static void SetupLabeledTextBox(ref TextBox textBox)
        {
            textBox.GotFocus += textBox_GotFocus;
            textBox.LostFocus += textBox_LostFocus;

            textBox.Text = textBox.Tag.ToString();
            textBox.FontStyle = Windows.UI.Text.FontStyle.Italic;
        }

        /// <summary>
        /// Change the text of a TextBox to its label when it loses focus, if it doesn't contain meaningful information
        /// </summary>
        private static void textBox_LostFocus(object sender, RoutedEventArgs e)
        {
            TextBox tBox = sender as TextBox;
            if (null == tBox)
                return;

            if (String.IsNullOrEmpty(tBox.Text) || 0 == String.Compare(tBox.Text, tBox.Tag.ToString()))
            {
                tBox.Text = tBox.Tag.ToString();
                tBox.FontStyle = Windows.UI.Text.FontStyle.Italic;
            }
        }

        /// <summary>
        /// Clears a TextBox of displayed text if it's displaying its label when it recieves focus
        /// </summary>
        private static void textBox_GotFocus(object sender, RoutedEventArgs e)
        {
            TextBox tBox = sender as TextBox;
            if (null == tBox)
                return;

            if (0 == String.Compare(tBox.Text, tBox.Tag.ToString()))
                tBox.Text = String.Empty;

            tBox.FontStyle = Windows.UI.Text.FontStyle.Normal;
        }
    }
}
