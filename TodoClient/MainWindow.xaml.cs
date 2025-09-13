// MainWindow.xaml.cs
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using TodoClient.Models;
using static System.Runtime.InteropServices.JavaScript.JSType;

namespace TodoClient
{
    public partial class MainWindow : Window
    {
        private TcpClient tcpClient;
        private StreamReader reader;
        private StreamWriter writer;

        public ObservableCollection<TodoItemModel> Items { get; } = new ObservableCollection<TodoItemModel>();

        public MainWindow()
        {
            InitializeComponent();
            TodoListView.ItemsSource = Items;
            Loaded += MainWindow_Loaded;
            Closing += MainWindow_Closing;
        }

        private async void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            await ConnectToServerAsync();
        }

        private async Task ConnectToServerAsync()
        {
            try
            {
                tcpClient = new TcpClient();
                await tcpClient.ConnectAsync("127.0.0.1", 5000);
                var stream = tcpClient.GetStream();
                reader = new StreamReader(stream, Encoding.UTF8);
                writer = new StreamWriter(stream, Encoding.UTF8) { AutoFlush = true };

                // Request initial list
                await writer.WriteLineAsync(JsonConvert.SerializeObject(new { action = "get" }));

                // Start listening for server messages
                _ = Task.Run(ListenLoop);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to connect to server: " + ex.Message);
            }
        }

        private async Task ListenLoop()
        {
            try
            {
                while (true)
                {
                    var line = await reader.ReadLineAsync();
                    if (line == null) break;

                    JToken token = JToken.Parse(line);
                    // If array => initial full list
                    if (token.Type == JTokenType.Array)
                    {
                        var arr = (JArray)token;
                        Application.Current.Dispatcher.Invoke(() =>
                        {
                            Items.Clear();
                            foreach (var it in arr)
                            {
                                Items.Add(new TodoItemModel
                                {
                                    Id = (int)it["id"],
                                    Description = (string)it["description"],
                                    IsCompleted = ((string)it["status"]) == "Completed"
                                });
                            }
                        });
                    }
                    else if (token.Type == JTokenType.Object)
                    {
                        var obj = (JObject)token;
                        if (obj.ContainsKey("error"))
                        {
                            var err = (string)obj["error"];
                            // show error as toast, skip
                            Application.Current.Dispatcher.Invoke(() =>
                            {
                                MessageBox.Show("Server: " + err);
                            });
                            continue;
                        }

                        int id = (int)obj["id"];
                        string desc = (string)obj["description"];
                        bool completed = ((string)obj["status"]) == "Completed";

                        Application.Current.Dispatcher.Invoke(() =>
                        {
                            var existing = Items.FirstOrDefault(x => x.Id == id);
                            if (existing != null)
                            {
                                existing.Description = desc;
                                existing.IsCompleted = completed;
                            }
                            else
                            {
                                Items.Add(new TodoItemModel { Id = id, Description = desc, IsCompleted = completed });
                            }
                        });
                    }
                }
            }
            catch (Exception ex)
            {
                Application.Current.Dispatcher.Invoke(() => MessageBox.Show("Connection lost: " + ex.Message));
            }
        }

        private async void AddButton_Click(object sender, RoutedEventArgs e)
        {
            var text = NewItemTextBox.Text?.Trim();
            if (string.IsNullOrEmpty(text)) return;

            try
            {
                var msg = JsonConvert.SerializeObject(new { action = "add", description = text });
                await writer.WriteLineAsync(msg);
                NewItemTextBox.Text = "";
                // We rely on server to broadcast the new item; when server replies we'll update UI
            }
            catch (Exception ex)
            {
                MessageBox.Show("Send failed: " + ex.Message);
            }
        }

        // The check/uncheck (Click) triggers this. We get the item from CheckBox.DataContext.
        private async void CheckBox_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var cb = sender as CheckBox;
                if (cb == null) return;
                if (!(cb.DataContext is TodoItemModel item)) return;

                var msg = JsonConvert.SerializeObject(new { action = "update", id = item.Id });
                await writer.WriteLineAsync(msg);
                // server will reply/broadcast canonical state
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to send update: " + ex.Message);
            }
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            try
            {
                reader?.Dispose();
                writer?.Dispose();
                tcpClient?.Close();
            }
            catch { }
        }
    }
}