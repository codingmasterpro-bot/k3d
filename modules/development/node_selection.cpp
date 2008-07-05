// K-3D
// Copyright (c) 1995-2008, Timothy M. Shead
//
// Contact: tshead@k-3d.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/** \file
		\author Bart Janssens (bart.janssens@lid.kviv.be)
 */

#include <map>
#include <vector>

#include <k3d-i18n-config.h>
#include <k3dsdk/high_res_timer.h>
#include <k3dsdk/inode_collection_sink.h>
#include <k3dsdk/inode_collection_property.h>
#include <k3dsdk/inode_name_map.h>
#include <k3dsdk/ipipeline.h>
#include <k3dsdk/mesh.h>
#include <k3dsdk/node.h>
#include <k3dsdk/document_plugin_factory.h>

namespace k3d
{

/// Interface for objects that store document node selections
class inode_selection :
	public virtual k3d::iunknown
{
public:
	/// Selects a node
	/**
	 * \param Node the node to select
	 * \param Weight The selection weight. Setting this to 0 removes the node from the selection
	 */
	virtual void select(inode& Node, const double_t Weight) = 0;

protected:
	inode_selection() {}
	inode_selection(const inode_selection&) {}
	inode_selection& operator=(const inode_selection&) { return *this; }
	virtual ~inode_selection() {}
};

}

namespace module
{

namespace development
{



class node_selection :
	public k3d::node,
	// public k3d::inode_collection_sink, // Not implementing this right now, since the UI layer automatically adds all nodes.
	public k3d::inode_selection
{
	typedef k3d::node base;
	typedef std::map<k3d::inode*, k3d::double_t> selection_t;
public:
	node_selection(k3d::iplugin_factory& Factory, k3d::idocument& Document) : base(Factory, Document),
		m_selected_nodes(init_owner(*this) + init_name("selected_nodes") + init_label(_("Selected Nodes")) + init_description(_("Ordered list of the selected nodes in the document")) + init_value(std::vector<k3d::inode*>())),
		m_selection_weights((init_owner(*this) + init_name("selection_weights") + init_label(_("Selection Weights")) + init_description(_("Selection weight for all nodes")) + init_value(selection_t()))),
		m_select_node(init_owner(*this) + init_name("select_node") + init_label(_("Select Node")) + init_description(_("Node to select")) + init_value<k3d::node*>(0))
	{
		m_select_node.changed_signal().connect(sigc::mem_fun(*this, &node_selection::on_select_node));
		m_selection_weight.changed_signal().connect(sigc::mem_fun(*this, &node_selection::on_select_node));
	}

	static k3d::iplugin_factory& get_factory()
	{
		static k3d::document_plugin_factory<node_selection,
			k3d::interface_list<k3d::inode> >factory(
			k3d::uuid(0x5d305922, 0xd2442343, 0x077956b6, 0xb112b9b3),
			"SelectedNodes",
			_("Stores the document node selection state"),
			"Development",
			k3d::iplugin_factory::EXPERIMENTAL);

		return factory;
	}

//	const k3d::inode_collection_sink::properties_t node_collection_properties()
//	{
//		return k3d::inode_collection_sink::properties_t(1, &m_selected_nodes);
//	}

	void select(k3d::inode& Node, const k3d::double_t Weight)
	{
		k3d::log() << debug << "setting weight of node " << Node.name() << " to " << Weight << std::endl;
		selection_t selection_weights = m_selection_weights.pipeline_value();
		k3d::inode_collection_property::nodes_t selected_nodes = m_selected_nodes.pipeline_value();
		if(Weight == 0.0) // deselect a node
		{
			selection_t::iterator selection_pair = selection_weights.find(&Node);
			if(selection_pair != selection_weights.end())
			{
				// Remove node from the selection weight map
				selection_weights.erase(selection_pair);
				// Remove node from the selected node list
				for(k3d::inode_collection_property::nodes_t::iterator node = selected_nodes.begin(); node != selected_nodes.end(); ++node)
				{
					if(*node == &Node)
					{
						selected_nodes.erase(node);
						break;
					}
				}
			}
		}
		else
		{
			// Append node to the end of the selected node list, unless it already was selected
			if(!selection_weights.count(&Node))
			{
				selected_nodes.push_back(&Node);
			}
			selection_weights[&Node] = Weight; // Set the selection weight
		}

		m_selected_nodes.set_value(selected_nodes);
		m_selection_weights.set_value(selection_weights);
	}

private:


	void on_select_node(k3d::ihint* Hint)
	{
		k3d::inode* node = m_select_node.pipeline_value();
		if(node)
			select(*node, m_selection_weight.pipeline_value());
	}


	template<typename value_t, class property_policy_t>
	class selection_weight_serialization :
		public property_policy_t,
		public ipersistent
	{
	public:
		void save(k3d::xml::element& Element, const k3d::ipersistent::save_context& Context)
		{
			std::stringstream buffer;

			const selection_t& selection = property_policy_t::internal_value();
			for(selection_t::const_iterator selection_pair = selection.begin(); selection_pair != selection.end(); ++selection_pair)
			{
				k3d::inode* node = selection_pair->first;
				k3d::double_t weight = selection_pair->second;
				if(node)
					buffer << " " << k3d::string_cast(Context.lookup.lookup_id(node));

				buffer << " " << weight;
			}

			Element.append(k3d::xml::element("property", buffer.str(), k3d::xml::attribute("name", property_policy_t::name())));
		}

		void load(k3d::xml::element& Element, const ipersistent::load_context& Context)
		{
			selection_t selection;

			std::stringstream buffer(Element.text);
			std::string node;
			std::string weight;
			while(buffer >> node)
			{
				return_if_fail(buffer >> weight);
				selection[dynamic_cast<inode*>(Context.lookup.lookup_object(k3d::from_string(node, static_cast<k3d::ipersistent_lookup::id_type>(0))))] = boost::lexical_cast<k3d::double_t>(weight);
			}

			property_policy_t::set_value(selection);
		}

	protected:
		template<typename init_t>
		selection_weight_serialization(const init_t& Init) :
			property_policy_t(Init)
		{
			Init.persistent_collection().enable_serialization(Init.name(), *this);
		}
	};

	k3d_data(k3d::inode_collection_property::nodes_t, immutable_name, change_signal, with_undo, local_storage, no_constraint, writable_property, node_collection_serialization) m_selected_nodes;
	k3d_data(selection_t, immutable_name, change_signal, with_undo, local_storage, no_constraint, writable_property, selection_weight_serialization) m_selection_weights;
	k3d_data(k3d::inode*, immutable_name, change_signal, with_undo, node_storage, no_constraint, node_property, node_serialization) m_select_node;
};

k3d::iplugin_factory& selected_nodes_factory()
{
	return node_selection::get_factory();
}

} // namespace development

} // namespace module
