#include <Nirvana/ModuleMetadata.h>
#include <Nirvana/OLF_Iterator.h>
#include <Nirvana/platform.h>
#include "pe_bliss/pe_lib/pe_bliss.h"
#include <iostream>
#include <stdexcept>

using namespace Nirvana;
using namespace pe_bliss;

class ModuleReader
{
public:
	ModuleReader (std::istream& file) :
		image_ (pe_factory::create_pe (file))
	{}

	ModuleReader (Nirvana::AccessBuf::_ptr_type file) :
		image_ (pe_factory::create_pe (file))
	{}

	Nirvana::ModuleMetadata get_module_metadata (bool exe) const
	{
		Nirvana::ModuleMetadata md;
		md.platform = image_.get_machine ();

		if (exe)
			md.type = ModuleType::MODULE_NIRVANA;
		else {
			uint32_t entry_point = image_.get_ep ();
			if (entry_point) {
				md.set_error ("Image must be linked with /NOENTRY");
				return md;
			}
		}

		const section* olf = nullptr;
		const section_list& sections = image_.get_image_sections ();
		for (const section& sec : sections) {
			if (sec.get_name () == OLF_BIND) {
				olf = &sec;
				break;
			}
		}

		if (!olf)
			md.set_error ("Metadata not found");
		else {
			bool valid;
			if (image_.get_pe_type () == pe_type_32)
				valid = iterate <uint32_t, false> (*olf, md);
			else
				valid = iterate <uint64_t, false> (*olf, md);

			if (!valid)
				md.set_error ("Invalid metadata");
		}

		return md;
	}

private:
	template <typename Word, bool other_endian>
	bool iterate (const section& olf, ModuleMetadata& md) const
	{
		typedef OLF_Iterator <Word, other_endian> Iterator;
		for (Iterator it (olf.get_raw_data ().data (), olf.get_raw_data ().size ()); !it.end (); it.next ()) {
			if (!it.valid ())
				return false;

			OLF_Command command = it.cur_command ();
			switch (command) {
			case OLF_IMPORT_INTERFACE:
			case OLF_IMPORT_OBJECT: {
				auto p = reinterpret_cast <const ImportInterfaceW <Word>*> (it.cur ());
				md.entries.push_back ({ command, 0, get_string (p->name), get_string (p->interface_id) });
			} break;

			case OLF_EXPORT_INTERFACE: {
				auto p = reinterpret_cast <const ExportInterfaceW <Word>*> (it.cur ());
				md.entries.push_back ({ command, 0, get_string (p->name), get_string (get_EPV <Word> (p->itf)->interface_id) });
			} break;

			case OLF_EXPORT_OBJECT:
			case OLF_EXPORT_LOCAL: {
				auto p = reinterpret_cast <const ExportObjectW <Word>*> (it.cur ());
				md.entries.push_back ({ command, 0, get_string (p->name), get_string (get_EPV <Word> (p->servant)->interface_id) });
			} break;

			case OLF_MODULE_STARTUP: {
				auto p = reinterpret_cast <const Nirvana::ModuleStartupW <Word>*> (it.cur ());
				md.entries.push_back ({ command, (unsigned)Iterator::native_endian (p->flags), get_string (p->name), get_string (get_EPV <Word> (p->startup)->interface_id) });
			} break;

			case OLF_PROCESS_STARTUP: {
				auto p = reinterpret_cast <const Nirvana::ModuleStartupW <Word>*> (it.cur ());
				md.entries.push_back ({ command, 0, std::string (), get_string (get_EPV <Word> (p->startup)->interface_id) });
			} break;
			}
		}

		return true;
	}

	const void* translate_addr (uint64_t va) const
	{
		return translate_rva (image_.va_to_rva (va));
	}

	const void* translate_addr (uint32_t va) const
	{
		return translate_rva (image_.va_to_rva (va));
	}

	const void* translate_rva (uint32_t rva) const
	{
		const section& sec = image_.section_from_rva (rva);
		return sec.get_raw_data ().data () + (rva - sec.get_virtual_address ());
	}

	const char* get_string (uint32_t va) const
	{
		return reinterpret_cast <const char*> (translate_addr (va));
	}

	const char* get_string (uint64_t va) const
	{
		return reinterpret_cast <const char*> (translate_addr (va));
	}

	template <typename Word>
	struct InterfaceEPV {
		Word interface_id;
	};

	template <typename Word>
	const InterfaceEPV <Word>* get_EPV (uint64_t itf) const
	{
		return reinterpret_cast <const InterfaceEPV <Word>*> (
			translate_addr (*reinterpret_cast <const Word*> (translate_addr (itf))));
	}

private:
	pe_bliss::pe_base image_;
};

namespace Nirvana {

ModuleMetadata get_module_metadata (std::istream& file, bool exe)
{
	ModuleMetadata md;
	try {
		ModuleReader reader (file);
		md = reader.get_module_metadata (exe);
	} catch (const std::exception& ex) {
		md.set_error (ex.what ());
	}
	return md;
}

ModuleMetadata get_module_metadata (Nirvana::AccessBuf::_ptr_type file, bool exe)
{
	ModuleMetadata md;
	try {
		ModuleReader reader (file);
		md = reader.get_module_metadata (exe);
	} catch (const std::exception& ex) {
		md.set_error (ex.what ());
	}
	return md;
}

}
