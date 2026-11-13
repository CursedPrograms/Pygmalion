@tool
extends EditorPlugin
# Pygmalion's link: Pygmalion sends a person to res://characters/<name>/ and then touches <name>.pygmalion.
# When that stamp changes, the person's files are imported again at once, so every scene using them updates while
# both programs are open. Put here by Pygmalion (Files > Export > Demiurge link).

const ROOT := "res://characters"
const KINDS := ["fbx", "glb", "gltf", "dae", "png", "jpg", "jpeg"]
var _seen := {}
var _busy := false  # importing: the timer can fire again meanwhile
var _timer: Timer


func _enter_tree() -> void:
	_timer = Timer.new()
	_timer.wait_time = 0.5
	_timer.autostart = true
	_timer.timeout.connect(_check)
	add_child(_timer)
	_check(true)


func _exit_tree() -> void:
	if _timer:
		_timer.queue_free()


func _check(first := false) -> void:
	if not DirAccess.dir_exists_absolute(ROOT):
		return
	var fs := EditorInterface.get_resource_filesystem()
	if _busy or fs.is_scanning():
		return
	_busy = true
	_scan(fs, first)
	_busy = false


func _scan(fs: EditorFileSystem, first: bool) -> void:
	for who in DirAccess.get_directories_at(ROOT):
		var dir := ROOT.path_join(who)
		var stamp := dir.path_join(who + ".pygmalion")
		if not FileAccess.file_exists(stamp):
			continue
		var t := FileAccess.get_modified_time(stamp)
		if _seen.get(stamp, -1) == t:
			continue
		_seen[stamp] = t
		if first:
			continue  # (at start-up the editor imports what changed by itself)
		var files := PackedStringArray()
		var unknown := false
		for f in DirAccess.get_files_at(dir):
			if f.get_extension().to_lower() in KINDS:
				var path := dir.path_join(f)
				files.append(path)
				if not FileAccess.file_exists(path + ".import"):
					unknown = true
		if unknown:
			fs.scan()  # new files: found and imported by the scan
		else:
			for f in files:
				fs.update_file(f)
			fs.reimport_files(files)
		print("Pygmalion: ", who, " updated")
