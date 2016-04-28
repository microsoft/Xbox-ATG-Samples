using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace errorlookup
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        #region Properties

        public TextBinding boundText = new TextBinding("Output here");
        System.Threading.Timer retrievalTick;
        System.Diagnostics.Stopwatch retrievalWatch;
        System.Threading.SynchronizationContext syncContext;
        int errorCodeBeingLookedUp = 0;
        bool currentlyLookingUpCode = false;

        #endregion

        /// <summary>
        /// Initialize the main form
        /// </summary>
        public MainPage()
        {
            this.InitializeComponent();

            // Set up UI and form controls
            ScrollViewer.SetVerticalScrollBarVisibility(textBox, ScrollBarVisibility.Visible);
            Utilities.SetupLabeledTextBox(ref codeInputBox);
            BitmapIcon icon = new BitmapIcon();
            icon.UriSource = new Uri("ms-appx:///Assets/arrow.png", UriKind.RelativeOrAbsolute);
            codeInputButton.Content = icon;

            // Set up for making sure UI updates are happening on this thread
            syncContext = System.Threading.SynchronizationContext.Current;
            retrievalTick = new System.Threading.Timer(RetrievalTick, null, System.Threading.Timeout.Infinite, 1000);

            // User interaction events
            SizeChanged += MainPage_SizeChanged;
            KeyDown += MainPage_KeyDown;
        }

        /// <summary>
        /// Capture the "Enter" key to lookup the error code
        /// </summary>
        private void MainPage_KeyDown(object sender, KeyRoutedEventArgs e)
        {
            // If the user hits the "Enter" key, treat that like a request to look up an error code
            if(e.Key == Windows.System.VirtualKey.Enter)
            {
                codeInputButton_Click(codeInputButton, null);
            }
        }

        /// <summary>
        /// Update the UI based on window resizes
        /// </summary>
        private void MainPage_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            // Stretch the output box to fill up available window space if it's resized
            textBox.Height = e.NewSize.Height - 200;
        }

        /// <summary>
        /// Periodically lets the user know an error code is being looked up if it's taking a while
        /// </summary>
        private void RetrievalTick(object state)
        {
            System.Diagnostics.Debug.WriteLine("Quertyng results...\n");
            boundText.Text = "Querying HResult...\n" + boundText.Text;
        }

        /// <summary>
        /// Indicates a button has been pressed by the user to intiate an error code lookup
        /// </summary>
        private void codeInputButton_Click(object sender, RoutedEventArgs e)
        {
            // Early exit if there's already another error code being looked up
            if (currentlyLookingUpCode)
                return;

            int errorCode = 0;
            retrievalWatch = System.Diagnostics.Stopwatch.StartNew();
            bool success = true;
            unchecked
            {
                try
                {
                    // Convert the error text from the input box into an int, and allow
                    //  hex input if it starts with "0x"
                    string textCode = codeInputBox.Text.Trim();
                    if (textCode.StartsWith("0x"))
                        errorCode = Convert.ToInt32(textCode.Substring(2), 16);
                    else
                        errorCode = Convert.ToInt32(codeInputBox.Text);
                    retrievalTick.Change(10, 1000);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine(ex.Message);
                    boundText.Text = ex.Message + "\n\n" + boundText.Text;
                    success = false;
                }
            }

            if (success)
            {
                currentlyLookingUpCode = true;

                // Update the three most recently looked up codes for quick access
                cachedCode2.Content = cachedCode1.Content;
                cachedCode1.Content = cachedCode0.Content;
                cachedCode0.Content = codeInputBox.Text.Trim();

                boundText.Text = "\n" + boundText.Text;

                // Kick off the async process to convert an error code to a user friendly string
                var result = Windows.Foundation.Diagnostics.ErrorDetails.CreateFromHResultAsync(errorCode);
                result.Completed += GetHRESULT;
                errorCodeBeingLookedUp = errorCode;
            }
        }

        /// <summary>
        /// Gets the results from querying the Cloud Error Message (CEM) service and prints it to the screen
        /// </summary>
        public async void GetHRESULT(Windows.Foundation.IAsyncOperation<Windows.Foundation.Diagnostics.ErrorDetails> info, AsyncStatus status)
        {
            retrievalTick.Change(System.Threading.Timeout.Infinite, 1000);
            try
            {
                // Write verbose information to the debug stream if there's a debugger attached
                var result = info.GetResults();
                System.Diagnostics.Debug.WriteLine("Got the HRESULT");
                System.Diagnostics.Debug.WriteLine("URI: " + result.HelpUri);
                System.Diagnostics.Debug.WriteLine("Description: " + result.Description);
                System.Diagnostics.Debug.WriteLine("Long Description: " + result.LongDescription);

                // Update the output box on the main thread
                await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                {
                    if (string.IsNullOrEmpty(result.Description))
                    {
                        boundText.Text = "Sorry, no result found\n" + boundText.Text;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Pushing error code");
                        string textToReport = "Retrieved error code in " + retrievalWatch.ElapsedMilliseconds + "ms" +
                        "\nError Code: " + errorCodeBeingLookedUp + 
                        "\nError Code: 0x" + errorCodeBeingLookedUp.ToString("X8") +
                        "\nError Message: " + result.Description;
                        System.Diagnostics.Debug.WriteLine(textToReport);
                        boundText.Text = textToReport + boundText.Text;
                    }
                });
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("There was a problem getting the HRESULT");
                System.Diagnostics.Debug.WriteLine(ex.Message);
            }

            // Done looking up an error code so allow the next lookup to occur
            currentlyLookingUpCode = false;
        }

        /// <summary>
        /// Initiates a lookup operation of a recently examined error code
        /// </summary>
        private void cachedCode_Click(object sender, RoutedEventArgs e)
        {
            Button button = (sender as Button);
            codeInputBox.Text = button.Content.ToString();

            var content0 = cachedCode0.Content;
            var content1 = cachedCode1.Content;
            var content2 = cachedCode2.Content;

            codeInputButton_Click(codeInputButton, null);

            cachedCode0.Content = content0;
            cachedCode1.Content = content1;
            cachedCode2.Content = content2;
        }
    }

    /// <summary>
    /// Helper class to update text bound to a control in C#
    /// </summary>
    public sealed class TextBinding : System.ComponentModel.INotifyPropertyChanged
    {
        private System.Threading.SynchronizationContext syncContext;

        public TextBinding(string text)
        {
            syncContext = System.Threading.SynchronizationContext.Current;
            Text = text;
        }

        public string Text
        {
            get { return _text; }
            set
            {
                syncContext.Post((o) =>
                {
                    _text = value;
                    OnPropertyChanged("Text");
                }, null);
            }
        }
        private string _text;

        public event PropertyChangedEventHandler PropertyChanged = delegate { };

        private void OnPropertyChanged(string propertyName = null)
        {
            System.Diagnostics.Debug.WriteLine("ReportData PropertyChanged");
            if (PropertyChanged != null)
                PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
