# Voice File Reference Tool 2
The source depends on the [Nana C++ GUI Library](http://nanapro.org/en-us/), and the [Cereal C++ Serialization Library](http://uscilab.github.io/cereal/).

The Nana library has been modified in the following ways:
* listbox.cpp:
  * `size_type sort_col()` changed to `std::pair<listbox::size_type, bool> sort_col()` - this function now returns both the sorted column, and the direction of sorting
  * `void es_lister::scroll_into_view(const index_pair&, view_action)` changed to center the item that's being scrolled into view
  * `item_proxy& item_proxy::select(bool, bool)` changed to scroll into view even if the item is already visible

* treebox.cpp:
  * `void trigger::mouse_wheel(graph_reference, const arg_wheel&)` changed to use the system setting for the number of lines scrolled at a time

* widget_iterator.hpp:
- made the class members public to correct what looks like a bug, and avoid the resulting build errors for VFRT2
